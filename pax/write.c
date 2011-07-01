/*-
 * Copyright (c) 2003-2010 Tim Kientzle
 * Copyright (c) 2011 Michihiro NAKAJIMA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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

#include "bsdpax_platform.h"
__FBSDID("$FreeBSD$");

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ATTR_XATTR_H
#include <attr/xattr.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LINUX_FS_H
#include <linux/fs.h>	/* for Linux file flags */
#endif
/*
 * Some Linux distributions have both linux/ext2_fs.h and ext2fs/ext2_fs.h.
 * As the include guards don't agree, the order of include is important.
 */
#ifdef HAVE_LINUX_EXT2_FS_H
#include <linux/ext2_fs.h>	/* for Linux file flags */
#endif
#if defined(HAVE_EXT2FS_EXT2_FS_H) && !defined(__CYGWIN__)
/* This header exists but is broken on Cygwin. */
#include <ext2fs/ext2_fs.h>
#endif
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "bsdpax.h"
#include "err.h"
#include "line_reader.h"

/* Fixed size of uname/gname caches. */
#define	name_cache_size 101

#ifndef O_BINARY
#define	O_BINARY 0
#endif

static const char * const NO_NAME = "(noname)";

struct archive_dir_entry {
	struct archive_dir_entry	*next;
	time_t			 mtime_sec;
	int			 mtime_nsec;
	time_t			 ctime_sec;
	int			 ctime_nsec;
	char			*name;
};

struct archive_dir {
	struct archive_dir_entry *head, *tail;
};

struct name_cache {
	int	probes;
	int	hits;
	size_t	size;
	struct {
		id_t id;
		const char *name;
	} cache[name_cache_size];
};

static void		 add_dir_list(struct bsdpax *bsdpax,
			    struct archive_entry*);
static int		 append_archive(struct bsdpax *, struct archive *,
			     struct archive *ina);
static int		 append_archive_filename(struct bsdpax *,
			     struct archive *, const char *fname);
static int		 copy_file_data_block(struct bsdpax *, struct archive *,
			     struct archive *, struct archive_entry *);
static int		 new_enough_mtime(struct bsdpax *,
			     struct archive_entry *entry);
static int		 new_enough_ctime(struct bsdpax *,
			     struct archive_entry *entry);
static void		 report_write(struct bsdpax *, struct archive *,
			     struct archive_entry *, int64_t progress);
static void		 test_for_append(struct bsdpax *);
static void		 write_archive(struct archive *, struct bsdpax *);
static void		 write_entry(struct bsdpax *, struct archive *,
			     struct archive_entry *);
static void		 write_file(struct bsdpax *, struct archive *,
			     struct archive_entry *);
static int		 write_hierarchy(struct bsdpax *, struct archive *,
			     const char *);

#if defined(_WIN32) && !defined(__CYGWIN__)
/* Not a full lseek() emulation, but enough for our needs here. */
static int
seek_file(int fd, int64_t offset, int whence)
{
	LARGE_INTEGER distance;
	(void)whence; /* UNUSED */
	distance.QuadPart = offset;
	return (SetFilePointerEx((HANDLE)_get_osfhandle(fd),
		distance, NULL, FILE_BEGIN) ? 1 : -1);
}
#define	open _open
#define	close _close
#define	read _read
#define	lseek seek_file
#endif

void
pax_mode_write(struct bsdpax *bsdpax)
{
	struct archive *a;
	int r;

	a = archive_write_new();

	/* Support any format that the library supports. */
	if (bsdpax->create_format == NULL) {
		r = archive_write_set_format_pax_restricted(a);
		bsdpax->create_format = "pax restricted";
	} else {
		r = archive_write_set_format_by_name(a, bsdpax->create_format);
	}
	if (r != ARCHIVE_OK) {
		fprintf(stderr, "Can't use format %s: %s\n",
		    bsdpax->create_format,
		    archive_error_string(a));
		usage();
	}

	archive_write_set_bytes_per_block(a, bsdpax->bytes_per_block);
	archive_write_set_bytes_in_last_block(a, bsdpax->bytes_in_last_block);

	if (bsdpax->compress_program) {
		archive_write_add_filter_program(a, bsdpax->compress_program);
	} else {
		switch (bsdpax->create_compression) {
		case 0:
			r = ARCHIVE_OK;
			break;
		case 'j':
			r = archive_write_add_filter_bzip2(a);
			break;
		case 'J':
			r = archive_write_add_filter_xz(a);
			break;
		case OPTION_LZIP:
			r = archive_write_add_filter_lzip(a);
			break;
		case OPTION_LZMA:
			r = archive_write_add_filter_lzma(a);
			break;
		case 'z':
			r = archive_write_add_filter_gzip(a);
			break;
		case OPTION_COMPRESS:
			r = archive_write_add_filter_compress(a);
			break;
		default:
			lafe_errc(1, 0,
			    "Unrecognized compression option -%c",
			    bsdpax->create_compression);
		}
		if (r != ARCHIVE_OK) {
			const char *zname;
			switch (bsdpax->create_compression) {
			case OPTION_LZIP: zname = "lzip"; break;
			case OPTION_LZMA: zname = "lzma"; break;
			case OPTION_COMPRESS: zname = "compress"; break;
			default: zname = NULL; break;
			}
			if (zname == NULL)
				lafe_errc(1, 0,
				    "Unsupported compression option -%c",
				    bsdpax->create_compression);
			else
				lafe_errc(1, 0,
				    "Unsupported compression option --%s",
				    zname);
		}
	}

	lafe_set_options(bsdpax->options, archive_write_set_options, a);
	if (ARCHIVE_OK != archive_write_open_file(a, bsdpax->filename))
		lafe_errc(1, 0, "%s", archive_error_string(a));
	write_archive(a, bsdpax);
}

void
pax_mode_append(struct bsdpax *bsdpax)
{
	int64_t			 end_offset;
	struct archive		*a;
	struct archive_entry	*entry;
	int			 format;
	struct archive_dir_entry	*p;
	struct archive_dir	 archive_dir;
	char			 need_list;

	bsdpax->archive_dir = &archive_dir;
	memset(&archive_dir, 0, sizeof(archive_dir));

	format = ARCHIVE_FORMAT_TAR_PAX_RESTRICTED;

	/* Sanity-test some arguments and the file. */
	test_for_append(bsdpax);

	bsdpax->fd = open(bsdpax->filename, O_RDWR | O_BINARY);
	if (bsdpax->fd < 0)
		lafe_errc(1, errno,
		    "Cannot open %s", bsdpax->filename);

	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_tar(a);
	archive_read_support_format_gnutar(a);
	if (archive_read_open_fd(a, bsdpax->fd, bsdpax->bytes_per_block)
	    != ARCHIVE_OK) {
		lafe_errc(1, 0,
		    "Can't open %s: %s", bsdpax->filename,
		    archive_error_string(a));
	}

	if (bsdpax->option_keep_newer_mtime_files_br ||
	    bsdpax->option_keep_newer_ctime_files_br ||
	    bsdpax->option_keep_newer_mtime_files_ar ||
	    bsdpax->option_keep_newer_ctime_files_ar)
		need_list = 1;
	else
		need_list = 0;
	/* Build a list of all entries and their recorded mod times. */
	while (0 == archive_read_next_header(a, &entry)) {
		if (archive_compression(a) != ARCHIVE_COMPRESSION_NONE) {
			archive_read_free(a);
			close(bsdpax->fd);
			lafe_errc(1, 0,
			    "Cannot append to compressed archive.");
		}
		if (need_list)
			add_dir_list(bsdpax, entry);
		/* Record the last format determination we see */
		format = archive_format(a);
		/* Keep going until we hit end-of-archive */
	}

	end_offset = archive_read_header_position(a);
	archive_read_free(a);

	/* Re-open archive for writing. */
	a = archive_write_new();
	archive_write_set_format(a, format);
	archive_write_set_bytes_per_block(a, bsdpax->bytes_per_block);
	archive_write_set_bytes_in_last_block(a, bsdpax->bytes_in_last_block);

	if (lseek(bsdpax->fd, end_offset, SEEK_SET) < 0)
		lafe_errc(1, errno, "Could not seek to archive end");
	lafe_set_options(bsdpax->options, archive_write_set_options, a);
	if (ARCHIVE_OK != archive_write_open_fd(a, bsdpax->fd))
		lafe_errc(1, 0, "%s", archive_error_string(a));

	write_archive(a, bsdpax);

	close(bsdpax->fd);
	bsdpax->fd = -1;

	while (bsdpax->archive_dir->head != NULL) {
		p = bsdpax->archive_dir->head->next;
		free(bsdpax->archive_dir->head->name);
		free(bsdpax->archive_dir->head);
		bsdpax->archive_dir->head = p;
	}
	bsdpax->archive_dir->tail = NULL;
}

/*
 * Write user-specified files/dirs to opened archive.
 */
static void
write_archive(struct archive *a, struct bsdpax *bsdpax)
{
	const char *arg;
	struct archive_entry *entry, *sparse_entry;

	/* Allocate a buffer for null data. */
	bsdpax->buff_size = 32 * 1024;
	if ((bsdpax->buff = malloc(bsdpax->buff_size)) == NULL)
		lafe_errc(1, 0, "cannot allocate memory");

	if ((bsdpax->resolver = archive_entry_linkresolver_new()) == NULL)
		lafe_errc(1, 0, "cannot create link resolver");
	archive_entry_linkresolver_set_strategy(bsdpax->resolver,
	    archive_format(a));

	/* Create and setup a disk reader. */
	if ((bsdpax->diskreader = archive_read_disk_new()) == NULL)
		lafe_errc(1, 0, "Cannot create read_disk object");
	/* Notify how handle symlink to the read_disk object. */
	switch (bsdpax->symlink_mode) {
	case 'H':
		archive_read_disk_set_symlink_hybrid(bsdpax->diskreader);
		break;
	case 'L':
		archive_read_disk_set_symlink_logical(bsdpax->diskreader);
		break;
	default:
		archive_read_disk_set_symlink_physical(bsdpax->diskreader);
		break;
	}
	if (bsdpax->option_restore_atime)
		archive_read_disk_set_atime_restored(bsdpax->diskreader);
	archive_read_disk_set_standard_lookup(bsdpax->diskreader);


	if (*bsdpax->argv == NULL) {
		/*
		 * Archive names specified in file.
		 */
		struct lafe_line_reader *lr;
		const char *line;

		lr = lafe_line_reader("-", bsdpax->option_null);
		while ((line = lafe_line_reader_next(lr)) != NULL) {
			if (write_hierarchy(bsdpax, a, line) < 0)
				break;
		}
		lafe_line_reader_free(lr);
	}
	while (*bsdpax->argv != NULL) {
		arg = *bsdpax->argv;
		if (bsdpax->mode == PAXMODE_WRITE && *arg == '@') {
			if (append_archive_filename(bsdpax, a, arg + 1) != 0)
				break;
		} else {
			if (write_hierarchy(bsdpax, a, arg) < 0)
				break;
		}
		bsdpax->argv++;
	}

	entry = NULL;
	archive_entry_linkify(bsdpax->resolver, &entry, &sparse_entry);
	while (entry != NULL) {
		int r = archive_read_disk_open(bsdpax->diskreader,
		    archive_entry_pathname(entry));
		if (r != ARCHIVE_OK) {
			lafe_warnc(archive_errno(bsdpax->diskreader),
			    "%s", archive_error_string(bsdpax->diskreader));
			if (r == ARCHIVE_FATAL)
				exit(1);
			else if (r < ARCHIVE_WARN) {
				bsdpax->return_value = 1;
				break;
			}
		}
		write_file(bsdpax, a, entry);
		archive_entry_free(entry);
		archive_read_close(bsdpax->diskreader);
		entry = NULL;
		archive_entry_linkify(bsdpax->resolver, &entry, &sparse_entry);
	}

	if (archive_write_close(a)) {
		lafe_warnc(0, "%s", archive_error_string(a));
		bsdpax->return_value = 1;
	}

	/* Free file data buffer. */
	free(bsdpax->buff);
	archive_entry_linkresolver_free(bsdpax->resolver);
	bsdpax->resolver = NULL;
	archive_read_free(bsdpax->diskreader);
	bsdpax->diskreader = NULL;

	if (bsdpax->verbose) {
		int i;

		fprintf(stderr, "%s: %s,", lafe_progname, archive_format_name(a));
		for (i = 0; archive_filter_code(a, i) > 0; i++) {
			if (i > 0)
				fprintf(stderr, "/%s", archive_filter_name(a, i));
			else
				fprintf(stderr, " %s", archive_filter_name(a, i));
		}
		if (i > 0)
			fprintf(stderr, " filters,");
		fprintf(stderr,
		    " %d files, 0 bytes read, %s bytes written\n",
		    archive_file_count(a),
		    pax_i64toa(archive_filter_bytes(a, -1)));
	}

	archive_write_free(a);
}

/*
 * Copy from specified archive to current archive.  Returns non-zero
 * for write errors (which force us to terminate the entire archiving
 * operation).  If there are errors reading the input archive, we set
 * bsdpax->return_value but return zero, so the overall archiving
 * operation will complete and return non-zero.
 */
static int
append_archive_filename(struct bsdpax *bsdpax, struct archive *a,
    const char *filename)
{
	struct archive *ina;
	int rc;

	if (strcmp(filename, "-") == 0)
		filename = NULL; /* Library uses NULL for stdio. */

	ina = archive_read_new();
	archive_read_support_format_all(ina);
	archive_read_support_filter_all(ina);
	if (archive_read_open_file(ina, filename, bsdpax->bytes_per_block)) {
		lafe_warnc(0, "%s", archive_error_string(ina));
		bsdpax->return_value = 1;
		return (0);
	}

	rc = append_archive(bsdpax, a, ina);

	if (rc != ARCHIVE_OK) {
		lafe_warnc(0, "Error reading archive %s: %s",
		    filename, archive_error_string(ina));
		bsdpax->return_value = 1;
	}
	archive_read_free(ina);

	return (rc);
}

static int
append_archive(struct bsdpax *bsdpax, struct archive *a, struct archive *ina)
{
	struct archive_entry *in_entry;
	int e, rename = 0;

	while (0 == archive_read_next_header(ina, &in_entry)) {
		/*
		 * Exclude entries that are older than archived one which
		 * has the same name. (-u, -D option.)
		 */
		if (bsdpax->option_keep_newer_mtime_files_br &&
		    !new_enough_mtime(bsdpax, in_entry))
			continue;
		if (bsdpax->option_keep_newer_ctime_files_br &&
		    !new_enough_ctime(bsdpax, in_entry))
			continue;
		/*
		 * Exclude entries that are specified.
		 */
		if (excluded_entry(bsdpax, in_entry))
			continue;/* Skip it. */

		if (bsdpax->option_interactive) {
			rename = pax_rename(bsdpax, in_entry);
			if (rename < 0)
				break; /* Do not add the following entries. */
			else if (rename == 0)
				continue;/* Skip this entry. */
		}

		/*
		 * Exclude entries that are older than archived one which
		 * has the same name. (-Z, -Y option.)
		 */
		if (bsdpax->option_keep_newer_mtime_files_ar &&
		    !new_enough_mtime(bsdpax, in_entry))
			continue;
		if (bsdpax->option_keep_newer_ctime_files_ar &&
		    !new_enough_ctime(bsdpax, in_entry))
			continue;

		if (bsdpax->verbose)
			safe_fprintf(stderr, "%s",
			    archive_entry_pathname(in_entry));
		if (need_report())
			report_write(bsdpax, a, in_entry, 0);

		/*
		 * Overrite attributes.
		 */
		lafe_edit_entry(bsdpax->options, in_entry);

		e = archive_write_header(a, in_entry);
		if (e != ARCHIVE_OK) {
			if (!bsdpax->verbose)
				lafe_warnc(0, "%s: %s",
				    archive_entry_pathname(in_entry),
				    archive_error_string(a));
			else
				fprintf(stderr, ": %s", archive_error_string(a));
		}
		if (e == ARCHIVE_FATAL)
			exit(1);

		if (e >= ARCHIVE_WARN) {
			if (archive_entry_size(in_entry) == 0)
				archive_read_data_skip(ina);
			else if (copy_file_data_block(bsdpax, a, ina, in_entry))
				exit(1);
		}

		if (bsdpax->verbose)
			fprintf(stderr, "\n");
	}

	/* Note: If we got here, we saw no write errors, so return success. */
	return (0);
}

/*
 * Add the file or dir hierarchy named by 'path' to the archive
 */
static int
write_hierarchy(struct bsdpax *bsdpax, struct archive *a, const char *path)
{
	struct archive *disk = bsdpax->diskreader;
	struct archive_entry *entry = NULL, *spare_entry = NULL;
	int first_fs = -1, r, rename = 0;

	r = archive_read_disk_open(disk, path);
	if (r != ARCHIVE_OK) {
		lafe_warnc(archive_errno(disk),
		    "%s", archive_error_string(disk));
		if (r == ARCHIVE_FATAL)
			exit(1);
		else if (r < ARCHIVE_WARN)
			return (-1);
	}

	for (;;) {
		archive_entry_free(entry);
		entry = archive_entry_new();
		r = archive_read_next_header2(disk, entry);
		if (r == ARCHIVE_EOF)
			break;
		else if (r != ARCHIVE_OK) {
			lafe_warnc(archive_errno(disk),
			    "%s", archive_error_string(disk));
			if (r == ARCHIVE_FATAL)
				exit(1);
			else if (r < ARCHIVE_WARN)
				continue;
		}

		/*
		 * Exclude entries that are specified.
		 */
		if (excluded_entry(bsdpax, entry))
			continue;/* Skip it. */

		/*
		 * Exclude entries that are older than archived one
		 * which has the same name. (-u, -D options)
		 */
		if (bsdpax->option_keep_newer_mtime_files_br &&
		    !new_enough_mtime(bsdpax, entry))
			continue;
		if (bsdpax->option_keep_newer_ctime_files_br &&
		    !new_enough_ctime(bsdpax, entry))
			continue;

		/*
		 * User has asked us not to cross mount points.
		 */
		if (bsdpax->option_dont_traverse_mounts) {
			if (first_fs == -1)
				first_fs =
				    archive_read_disk_current_filesystem(disk);
			else if (first_fs !=
			    archive_read_disk_current_filesystem(disk))
				continue;
		}

		/*
		 * If this file/dir is flagged "nodump" and we're
		 * honoring such flags, skip this file/dir.
		 */
#if defined(HAVE_STRUCT_STAT_ST_FLAGS) && defined(UF_NODUMP)
		if (bsdpax->option_honor_nodump) {
			unsigned long flags, clear;
			archive_entry_fflags(entry, &flags, &clear);
			/* BSD systems store flags in struct stat */
			if (flags & UF_NODUMP)
				continue;
		}
#endif
#if defined(EXT2_IOC_GETFLAGS) && defined(EXT2_NODUMP_FL)
		if (bsdpax->option_honor_nodump) {
			unsigned long flags, clear;
			archive_entry_fflags(entry, &flags, &clear);
			if (flags & EXT2_NODUMP_FL)
				continue;
		}
#endif

#ifdef __APPLE__
		if (bsdpax->enable_copyfile) {
			/* If we're using copyfile(), ignore "._XXX" files. */
			const char *bname =
			    strrchr(archive_entry_pathname(entry), '/');
			if (bname == NULL)
				bname = archive_entry_pathname(entry);
			else
				++bname;
			if (bname[0] == '.' && bname[1] == '_')
				continue;
		} else {
			/* If not, drop the copyfile() data. */
			archive_entry_copy_mac_metadata(entry, NULL, 0);
		}
#endif

		/*
		 * Rewrite the pathname to be archived.  If rewrite
		 * fails, skip the entry.
		 */
		if (edit_pathname(bsdpax, entry))
			continue;

		/*
		 * If the user vetoes this file/directory, skip it.
		 * We want this to be fairly late; if some other
		 * check would veto this file, we shouldn't bother
		 * the user with it.
		 */
		if (bsdpax->option_interactive) {
			rename = pax_rename(bsdpax, entry);
			if (rename < 0)
				break; /* Do not add the following entries. */
			else if (rename == 0)
				continue;/* Skip this entry. */
		}

		/*
		 * Exclude entries that are older than archived one which
		 * has the same name. (-Z, -Y options)
		 */
		if (bsdpax->option_keep_newer_mtime_files_ar &&
		    !new_enough_mtime(bsdpax, entry))
			continue;
		if (bsdpax->option_keep_newer_ctime_files_ar &&
		    !new_enough_ctime(bsdpax, entry))
			continue;

		/* Note: if user vetoes, we won't descend. */
		if (!bsdpax->option_no_subdirs)
			archive_read_disk_descend(disk);

		/* Display entry as we process it. */
		if (bsdpax->verbose)
			safe_fprintf(stderr, "%s",
			    archive_entry_pathname(entry));

		/* Non-regular files get archived with zero size. */
		if (archive_entry_filetype(entry) != AE_IFREG)
			archive_entry_set_size(entry, 0);

		/*
		 * Overrite attributes.
		 */
		lafe_edit_entry(bsdpax->options, entry);

		archive_entry_linkify(bsdpax->resolver, &entry, &spare_entry);

		while (entry != NULL) {
			write_file(bsdpax, a, entry);
			archive_entry_free(entry);
			entry = spare_entry;
			spare_entry = NULL;
		}

		if (bsdpax->verbose)
			fprintf(stderr, "\n");
	}
	archive_entry_free(entry);
	archive_read_close(disk);

	return (rename);
}

/*
 * Write a single file (or directory or other filesystem object) to
 * the archive.
 */
static void
write_file(struct bsdpax *bsdpax, struct archive *a, struct archive_entry *entry)
{
	write_entry(bsdpax, a, entry);
}

/*
 * Write a single entry to the archive.
 */
static void
write_entry(struct bsdpax *bsdpax, struct archive *a,
    struct archive_entry *entry)
{
	int e;

	e = archive_write_header(a, entry);
	if (e != ARCHIVE_OK) {
		if (!bsdpax->verbose)
			lafe_warnc(0, "%s: %s",
			    archive_entry_pathname(entry),
			    archive_error_string(a));
		else
			fprintf(stderr, ": %s", archive_error_string(a));
	}

	if (e == ARCHIVE_FATAL)
		exit(1);

	/*
	 * If we opened a file earlier, write it out now.  Note that
	 * the format handler might have reset the size field to zero
	 * to inform us that the archive body won't get stored.  In
	 * that case, just skip the write.
	 */
	if (e >= ARCHIVE_WARN && archive_entry_size(entry) > 0) {
		if (copy_file_data_block(bsdpax, a, bsdpax->diskreader, entry))
			exit(1);
	}
}

static void
report_write(struct bsdpax *bsdpax, struct archive *a,
    struct archive_entry *entry, int64_t progress)
{
	uint64_t comp, uncomp;
	int compression;

	if (bsdpax->verbose)
		fprintf(stderr, "\n");
	comp = archive_position_compressed(a);
	uncomp = archive_position_uncompressed(a);
	fprintf(stderr, "In: %d files, %s bytes;",
	    archive_file_count(a), pax_i64toa(uncomp));
	if (comp > uncomp)
		compression = 0;
	else
		compression = (int)((uncomp - comp) * 100 / uncomp);
	fprintf(stderr,
	    " Out: %s bytes, compression %d%%\n",
	    pax_i64toa(comp), compression);
	/* Can't have two calls to pax_i64toa() pending, so split the output. */
	safe_fprintf(stderr, "Current: %s (%s",
	    archive_entry_pathname(entry),
	    pax_i64toa(progress));
	fprintf(stderr, "/%s bytes)\n",
	    pax_i64toa(archive_entry_size(entry)));
}


/* Helper function to copy file to archive. */
static int
copy_file_data_block(struct bsdpax *bsdpax, struct archive *a,
    struct archive *in_a, struct archive_entry *entry)
{
	size_t	bytes_read;
	ssize_t	bytes_written;
	int64_t	offset, progress = 0;
	char *null_buff = NULL;
	const void *buff;
	int r;

	while ((r = archive_read_data_block(in_a, &buff,
	    &bytes_read, &offset)) == ARCHIVE_OK) {
		if (need_report())
			report_write(bsdpax, a, entry, progress);

		if (offset < progress) {
			int64_t sparse = progress - offset;
			size_t ns;

			if (null_buff == NULL) {
				null_buff = bsdpax->buff;
				memset(null_buff, 0, bsdpax->buff_size);
			}

			while (sparse > 0) {
				if (sparse > bsdpax->buff_size)
					ns = bsdpax->buff_size;
				else
					ns = (size_t)sparse;
				bytes_written =
				    archive_write_data(a, null_buff, ns);
				if (bytes_written < 0) {
					/* Write failed; this is bad */
					lafe_warnc(0, "%s",
					     archive_error_string(a));
					return (-1);
				}
				if ((size_t)bytes_written < ns) {
					/* Write was truncated; warn but
					 * continue. */
					lafe_warnc(0,
					    "%s: Truncated write; file may "
					    "have grown while being archived.",
					    archive_entry_pathname(entry));
					return (0);
				}
				progress += bytes_written;
				sparse -= bytes_written;
			}
		}

		bytes_written = archive_write_data(a, buff, bytes_read);
		if (bytes_written < 0) {
			/* Write failed; this is bad */
			lafe_warnc(0, "%s", archive_error_string(a));
			return (-1);
		}
		if ((size_t)bytes_written < bytes_read) {
			/* Write was truncated; warn but continue. */
			lafe_warnc(0,
			    "%s: Truncated write; file may have grown "
			    "while being archived.",
			    archive_entry_pathname(entry));
			return (0);
		}
		progress += bytes_written;
	}
	if (r < ARCHIVE_WARN) {
		lafe_warnc(archive_errno(a), "%s", archive_error_string(a));
		return (-1);
	}
	return (0);
}

/*
 * Test if the specified file is new enough to include in the archive.
 */
static int
new_enough_mtime(struct bsdpax *bsdpax, struct archive_entry *entry)
{
	struct archive_dir_entry *p;
	const char * path = archive_entry_pathname(entry);

	/*
	 * In -u mode, we only write an entry if it's newer than
	 * what was already in the archive.
	 */
	if (bsdpax->archive_dir != NULL &&
	    bsdpax->archive_dir->head != NULL) {
		for (p = bsdpax->archive_dir->head; p != NULL; p = p->next) {
			if (pathcmp(path, p->name)==0)
				return (p->mtime_sec < archive_entry_mtime(entry));
		}
	}

	/* If the file wasn't rejected, include it. */
	return (1);
}

static int
new_enough_ctime(struct bsdpax *bsdpax, struct archive_entry *entry)
{
	struct archive_dir_entry *p;
	const char * path = archive_entry_pathname(entry);

	/*
	 * In -u mode, we only write an entry if it's newer than
	 * what was already in the archive.
	 */
	if (bsdpax->archive_dir != NULL &&
	    bsdpax->archive_dir->head != NULL) {
		for (p = bsdpax->archive_dir->head; p != NULL; p = p->next) {
			if (pathcmp(path, p->name)==0)
				return (p->ctime_sec < archive_entry_ctime(entry));
		}
	}

	/* If the file wasn't rejected, include it. */
	return (1);
}

/*
 * Add an entry to the dir list for 'u' mode.
 *
 * XXX TODO: Make this fast.
 */
static void
add_dir_list(struct bsdpax *bsdpax, struct archive_entry *entry)
{
	struct archive_dir_entry	*p;
	const char *path = archive_entry_pathname(entry);
	time_t mtime_sec = archive_entry_mtime(entry);
	int mtime_nsec = archive_entry_mtime_nsec(entry);
	time_t ctime_sec = archive_entry_ctime(entry);
	int ctime_nsec = archive_entry_ctime_nsec(entry);

	/*
	 * Search entire list to see if this file has appeared before.
	 * If it has, override the timestamp data.
	 */
	p = bsdpax->archive_dir->head;
	while (p != NULL) {
		if (strcmp(path, p->name)==0) {
			p->mtime_sec = mtime_sec;
			p->mtime_nsec = mtime_nsec;
			p->ctime_sec = ctime_sec;
			p->ctime_nsec = ctime_nsec;
			return;
		}
		p = p->next;
	}

	p = malloc(sizeof(*p));
	if (p == NULL)
		lafe_errc(1, ENOMEM, "Can't read archive directory");

	p->name = strdup(path);
	if (p->name == NULL)
		lafe_errc(1, ENOMEM, "Can't read archive directory");
	p->mtime_sec = mtime_sec;
	p->mtime_nsec = mtime_nsec;
	p->ctime_sec = ctime_sec;
	p->ctime_nsec = ctime_nsec;
	p->next = NULL;
	if (bsdpax->archive_dir->tail == NULL) {
		bsdpax->archive_dir->head = bsdpax->archive_dir->tail = p;
	} else {
		bsdpax->archive_dir->tail->next = p;
		bsdpax->archive_dir->tail = p;
	}
}

static void
test_for_append(struct bsdpax *bsdpax)
{
	struct stat s;

	if (bsdpax->filename == NULL)
		lafe_errc(1, 0, "Cannot append to stdout.");

	if (bsdpax->create_compression != 0)
		lafe_errc(1, 0,
		    "Cannot append to %s with compression", bsdpax->filename);

	if (stat(bsdpax->filename, &s) != 0)
		return;

	if (!S_ISREG(s.st_mode) && !S_ISBLK(s.st_mode))
		lafe_errc(1, 0,
		    "Cannot append to %s: not a regular file.",
		    bsdpax->filename);

/* Is this an appropriate check here on Windows? */
/*
	if (GetFileType(handle) != FILE_TYPE_DISK)
		lafe_errc(1, 0, "Cannot append");
*/

}

