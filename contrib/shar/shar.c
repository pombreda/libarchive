/*-
 * Copyright (c) 2008 Jaakko Heinonen
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifdef __FBSDID
__FBSDID("$FreeBSD$");
#endif

#include <sys/stat.h>
#include <sys/types.h>

#include <transform.h>
#include <transform_entry.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "tree.h"

/* command line options */
static int	b_opt;	/* use alternative shar binary format */
static int	r_opt;	/* recurse into subdirectories */
static char	*o_arg;	/* output file name */

static void
usage(void)
{
	fprintf(stderr, "Usage: shar [-br] [-o filename] file ...\n");
	exit(EX_USAGE);
}

/*
 * Initialize transform structure and create a shar transform.
 */
static struct transform *
shar_create(void)
{
	struct transform *a;

	if ((a = transform_write_new()) == NULL)
		errx(EXIT_FAILURE, "%s", transform_error_string(a));

	if (b_opt)
		transform_write_set_format_shar_dump(a);
	else
		transform_write_set_format_shar(a);
	transform_write_set_compression_none(a);

	if (transform_write_open_filename(a, o_arg) != TRANSFORM_OK)
		errx(EX_CANTCREAT, "%s", transform_error_string(a));

	return (a);
}

/* buffer for file data */
static char buffer[32768];

/*
 * Write file data to an transform entry.
 */
static int
shar_write_entry_data(struct transform *a, const int fd)
{
	ssize_t bytes_read, bytes_written;

	assert(a != NULL);
	assert(fd >= 0);

	bytes_read = read(fd, buffer, sizeof(buffer));
	while (bytes_read != 0) {
		if (bytes_read < 0) {
			transform_set_error(a, errno, "Read failed");
			return (TRANSFORM_WARN);
		}
		bytes_written = transform_write_data(a, buffer, bytes_read);
		if (bytes_written < 0)
			return (TRANSFORM_WARN);
		bytes_read = read(fd, buffer, sizeof(buffer));
	}

	return (TRANSFORM_OK);
}

/*
 * Write a file to the transform. We have special handling for symbolic links.
 */
static int
shar_write_entry(struct transform *a, const char *pathname, const char *accpath,
    const struct stat *st)
{
	struct transform_entry *entry;
	int fd = -1;
	int ret = TRANSFORM_OK;

	assert(a != NULL);
	assert(pathname != NULL);
	assert(accpath != NULL);
	assert(st != NULL);

	entry = transform_entry_new();

	if (S_ISREG(st->st_mode) && st->st_size > 0) {
		/* regular file */
		if ((fd = open(accpath, O_RDONLY)) == -1) {
			warn("%s", accpath);
			ret = TRANSFORM_WARN;
			goto out;
		}
	} else if (S_ISLNK(st->st_mode)) {
		/* symbolic link */
		char lnkbuff[PATH_MAX + 1];
		int lnklen;
		if ((lnklen = readlink(accpath, lnkbuff, PATH_MAX)) == -1) {
			warn("%s", accpath);
			ret = TRANSFORM_WARN;
			goto out;
		}
		lnkbuff[lnklen] = '\0';
		transform_entry_set_symlink(entry, lnkbuff);
	}
	transform_entry_copy_stat(entry, st);
	transform_entry_set_pathname(entry, pathname);
	if (!S_ISREG(st->st_mode) || st->st_size == 0)
		transform_entry_set_size(entry, 0);
	if (transform_write_header(a, entry) != TRANSFORM_OK) {
		warnx("%s: %s", pathname, transform_error_string(a));
		ret = TRANSFORM_WARN;
		goto out;
	}
	if (fd >= 0) {
		if ((ret = shar_write_entry_data(a, fd)) != TRANSFORM_OK)
			warnx("%s: %s", accpath, transform_error_string(a));
	}
out:
	transform_entry_free(entry);
	if (fd >= 0)
		close(fd);

	return (ret);
}

/*
 * Write singe path to the transform. The path can be a regular file, directory
 * or device. Symbolic links are followed.
 */
static int
shar_write_path(struct transform *a, const char *pathname)
{
	struct stat st;

	assert(a != NULL);
	assert(pathname != NULL);

	if ((stat(pathname, &st)) == -1) {
		warn("%s", pathname);
		return (TRANSFORM_WARN);
	}

	return (shar_write_entry(a, pathname, pathname, &st));
}

/*
 * Write tree to the transform. If pathname is a symbolic link it will be
 * followed. Other symbolic links are stored as such to the transform.
 */
static int
shar_write_tree(struct transform *a, const char *pathname)
{
	struct tree *t;
	const struct stat *lst, *st;
	int error = 0;
	int tree_ret;
	int first;

	assert(a != NULL);
	assert(pathname != NULL);

	t = tree_open(pathname);
	for (first = 1; (tree_ret = tree_next(t)); first = 0) {
		if (tree_ret == TREE_ERROR_DIR) {
			warnx("%s: %s", tree_current_path(t),
			    strerror(tree_errno(t)));
			error = 1;
			continue;
		} else if (tree_ret != TREE_REGULAR)
			continue;
		if ((lst = tree_current_lstat(t)) == NULL) {
			warn("%s", tree_current_path(t));
			error = 1;
			continue;
		}
		/*
		 * If the symlink was given on command line then
		 * follow it rather than write it as symlink.
		 */
		if (first && S_ISLNK(lst->st_mode)) {
			if ((st = tree_current_stat(t)) == NULL) {
				warn("%s", tree_current_path(t));
				error = 1;
				continue;
			}
		} else
			st = lst;

		if (shar_write_entry(a, tree_current_path(t),
		    tree_current_access_path(t), st) != TRANSFORM_OK)
			error = 1;

		tree_descend(t);
	}

	tree_close(t);

	return ((error != 0) ? TRANSFORM_WARN : TRANSFORM_OK);
}

/*
 * Create a shar transform and write files/trees into it.
 */
static int
shar_write(char **fn, size_t nfn)
{
	struct transform *a;
	size_t i;
	int error = 0;

	assert(fn != NULL);
	assert(nfn > 0);

	a = shar_create();

	for (i = 0; i < nfn; i++) {
		if (r_opt) {
			if (shar_write_tree(a, fn[i]) !=  TRANSFORM_OK)
				error = 1;
		} else {
			if (shar_write_path(a, fn[i]) != TRANSFORM_OK)
				error = 1;
		}
	}

	if (transform_write_free(a) != TRANSFORM_OK)
		errx(EXIT_FAILURE, "%s", transform_error_string(a));

	if (error != 0)
		warnx("Error exit delayed from previous errors.");

	return (error);
}

int
main(int argc, char **argv)
{
	int opt;

	while ((opt = getopt(argc, argv, "bro:")) != -1) {
		switch (opt) {
		case 'b':
			b_opt = 1;
			break;
		case 'o':
			o_arg = optarg;
			break;
		case 'r':
			r_opt = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if(argc < 1)
		usage();

	if (shar_write(argv, argc) != 0)
		exit(EXIT_FAILURE);
	else
		exit(EXIT_SUCCESS);
	/* NOTREACHED */
}

