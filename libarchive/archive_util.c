/*-
 * Copyright (c) 2009,2010 Michihiro NAKAJIMA
 * Copyright (c) 2003-2007 Tim Kientzle
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

#include "archive_platform.h"
__FBSDID("$FreeBSD: head/lib/libarchive/archive_util.c 201098 2009-12-28 02:58:14Z kientzle $");

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_string.h"

#if ARCHIVE_VERSION_NUMBER < 3000000
/* These disappear in libarchive 3.0 */
/* Deprecated. */
int
archive_api_feature(void)
{
	return (ARCHIVE_API_FEATURE);
}

/* Deprecated. */
int
archive_api_version(void)
{
	return (ARCHIVE_API_VERSION);
}

/* Deprecated synonym for archive_version_number() */
int
archive_version_stamp(void)
{
	return (archive_version_number());
}

/* Deprecated synonym for archive_version_string() */
const char *
archive_version(void)
{
	return (archive_version_string());
}
#endif

int
archive_version_number(void)
{
	return (ARCHIVE_VERSION_NUMBER);
}

const char *
archive_version_string(void)
{
	return (ARCHIVE_VERSION_STRING);
}

int
archive_errno(struct archive *a)
{
	return (a->archive_error_number);
}

const char *
archive_error_string(struct archive *a)
{

	if (a->error != NULL  &&  *a->error != '\0')
		return (a->error);
	else
		return ("(Empty error message)");
}

int
archive_file_count(struct archive *a)
{
	return (a->file_count);
}

int
archive_format(struct archive *a)
{
	return (a->archive_format);
}

const char *
archive_format_name(struct archive *a)
{
	return (a->archive_format_name);
}


int
archive_compression(struct archive *a)
{
	return archive_filter_code(a, 0);
}

const char *
archive_compression_name(struct archive *a)
{
	return archive_filter_name(a, 0);
}


/*
 * Return a count of the number of compressed bytes processed.
 */
int64_t
archive_position_compressed(struct archive *a)
{
	return archive_filter_bytes(a, -1);
}

/*
 * Return a count of the number of uncompressed bytes processed.
 */
int64_t
archive_position_uncompressed(struct archive *a)
{
	return archive_filter_bytes(a, 0);
}

void
archive_clear_error(struct archive *a)
{
	archive_string_empty(&a->error_string);
	a->error = NULL;
	a->archive_error_number = 0;
}

void
archive_set_error(struct archive *a, int error_number, const char *fmt, ...)
{
	va_list ap;

	a->archive_error_number = error_number;
	if (fmt == NULL) {
		a->error = NULL;
		return;
	}

	va_start(ap, fmt);
	archive_string_vsprintf(&(a->error_string), fmt, ap);
	va_end(ap);
	a->error = a->error_string.s;
}

void
archive_copy_error(struct archive *dest, struct archive *src)
{
	dest->archive_error_number = src->archive_error_number;

	archive_string_copy(&dest->error_string, &src->error_string);
	dest->error = dest->error_string.s;
}

void
__archive_errx(int retvalue, const char *msg)
{
	static const char *msg1 = "Fatal Internal Error in libarchive: ";
	size_t s;

	s = write(2, msg1, strlen(msg1));
	(void)s; /* UNUSED */
	s = write(2, msg, strlen(msg));
	(void)s; /* UNUSED */
	s = write(2, "\n", 1);
	(void)s; /* UNUSED */
	exit(retvalue);
}

/*
 * Parse option strings
 *  Detail of option format.
 *    - The option can accept:
 *     "opt-name", "!opt-name", "opt-name=value".
 *
 *    - The option entries are separated by comma.
 *        e.g  "compression=9,opt=XXX,opt-b=ZZZ"
 *
 *    - The name of option string consist of '-' and alphabet
 *      but character '-' cannot be used for the first character.
 *      (Regular expression is [a-z][-a-z]+)
 *
 *    - For a specfic format/filter, using the format name with ':'.
 *        e.g  "zip:compression=9"
 *        (This "compression=9" option entry is for "zip" format only)
 *
 *      If another entries follow it, those are not for
 *      the specfic format/filter.
 *        e.g  handle "zip:compression=9,opt=XXX,opt-b=ZZZ"
 *          "zip" format/filter handler will get "compression=9"
 *          all format/filter handler will get "opt=XXX"
 *          all format/filter handler will get "opt-b=ZZZ"
 *
 *    - Whitespace and tab are bypassed.
 *
 */
int
__archive_parse_options(const char *p, const char *fn, int keysize, char *key,
    int valsize, char *val)
{
	const char *p_org;
	int apply;
	int kidx, vidx;
	int negative; 
	enum {
		/* Requested for initialization. */
		INIT,
		/* Finding format/filter-name and option-name. */
		F_BOTH,
		/* Finding option-name only.
		 * (already detected format/filter-name) */
		F_NAME,
		/* Getting option-value. */
		G_VALUE,
	} state;

	p_org = p;
	state = INIT;
	kidx = vidx = negative = 0;
	apply = 1;
	while (*p) {
		switch (state) {
		case INIT:
			kidx = vidx = 0;
			negative = 0;
			apply = 1;
			state = F_BOTH;
			break;
		case F_BOTH:
		case F_NAME:
			if ((*p >= 'a' && *p <= 'z') ||
			    (*p >= '0' && *p <= '9') || *p == '-') {
				if (kidx == 0 && !(*p >= 'a' && *p <= 'z'))
					/* Illegal sequence. */
					return (-1);
				if (kidx >= keysize -1)
					/* Too many characters. */
					return (-1);
				key[kidx++] = *p++;
			} else if (*p == '!') {
				if (kidx != 0)
					/* Illegal sequence. */
					return (-1);
				negative = 1;
				++p;
			} else if (*p == ',') {
				if (kidx == 0)
					/* Illegal sequence. */
					return (-1);
				if (!negative)
					val[vidx++] = '1';
				/* We have got boolean option data. */
				++p;
				if (apply)
					goto complete;
				else
					/* This option does not apply to the
					 * format which the fn variable
					 * indicate. */
					state = INIT;
			} else if (*p == ':') {
				/* obuf data is format name */
				if (state == F_NAME)
					/* We already found it. */
					return (-1);
				if (kidx == 0)
					/* Illegal sequence. */
					return (-1);
				if (negative)
					/* We cannot accept "!format-name:". */
					return (-1);
				key[kidx] = '\0';
				if (strcmp(fn, key) != 0)
					/* This option does not apply to the
					 * format which the fn variable
					 * indicate. */
					apply = 0;
				kidx = 0;
				++p;
				state = F_NAME;
			} else if (*p == '=') {
				if (kidx == 0)
					/* Illegal sequence. */
					return (-1);
				if (negative)
					/* We cannot accept "!opt-name=value". */
					return (-1);
				++p;
				state = G_VALUE;
			} else if (*p == ' ') {
				/* Pass the space character */
				++p;
			} else {
				/* Illegal character. */
				return (-1);
			}
			break;
		case G_VALUE:
			if (*p == ',') {
				if (vidx == 0)
					/* Illegal sequence. */
					return (-1);
				/* We have got option data. */
				++p;
				if (apply)
					goto complete;
				else
					/* This option does not apply to the
					 * format which the fn variable
					 * indicate. */
					state = INIT;
			} else if (*p == ' ') {
				/* Pass the space character */
				++p;
			} else {
				if (vidx >= valsize -1)
					/* Too many characters. */
					return (-1);
				val[vidx++] = *p++;
			}
			break;
		} 
	}

	switch (state) {
	case F_BOTH:
	case F_NAME:
		if (kidx != 0) {
			if (!negative)
				val[vidx++] = '1';
			/* We have got boolean option. */
			if (apply)
				/* This option apply to the format which the
				 * fn variable indicate. */
				goto complete;
		}
		break;
	case G_VALUE:
		if (vidx == 0)
			/* Illegal sequence. */
			return (-1);
		/* We have got option value. */
		if (apply)
			/* This option apply to the format which the fn
			 * variable indicate. */
			goto complete;
		break;
	case INIT:/* nothing */
		break;
	}

	/* End of Option string. */
	return (0);

complete:
	key[kidx] = '\0';
	val[vidx] = '\0';
	/* Return a size which we've consumed for detecting option */
	return ((int)(p - p_org));
}

/*
 * Create a temporary file
 */
#if !defined(_WIN32) || defined(__CYGWIN__)

static int
get_tempdir(struct archive_string *temppath)
{
	const char *tmp;

	tmp = getenv("TMPDIR");
	if (tmp == NULL)
#ifdef _PATH_TMP
		tmp = _PATH_TMP;
#else
                tmp = "/tmp";
#endif
	archive_strcpy(temppath, tmp);
	if (temppath->s[temppath->length-1] != '/')
		archive_strappend_char(temppath, '/');
	return (ARCHIVE_OK);
}

void
__archive_set_error_from_transform(struct archive *a, struct transform *t)
{
	/* XXX optimization, transfer the error_string only if it isn't the NULL default */
 	archive_set_error(a, ARCHIVE_ERRNO_TRANSFORM,
 		"transform error: errno %d, error %s",
 		transform_errno(t), transform_error_string(t));
}

int
__convert_transform_fatal_error(struct archive *a, int result)
{
	if (TRANSFORM_FATAL != result)
		return (result);
	__archive_set_error_from_transform(a, a->transform);
	return (ARCHIVE_FATAL);
}

int
__convert_transform_error_to_archive_error(struct archive *a,
    struct transform *t, int error)
{
	if (TRANSFORM_OK == error)
		return (ARCHIVE_OK);

	if (TRANSFORM_EOF == error)
		return (ARCHIVE_EOF);

	/* no other states possible, thus transfer the error data. from the transform*/
	__archive_set_error_from_transform(a, t);

	/* XXX should we be upgrading unknown states to FATAL? */

	if (TRANSFORM_WARN == error) {
		return (ARCHIVE_WARN);
	} else if (TRANSFORM_FATAL == error) {
		return (ARCHIVE_FATAL);
	} else {
		archive_set_error(a, ARCHIVE_ERRNO_PROGRAMMER,
			"unknown transform return(%d), errno(%d), error(%s)",
			error, transform_errno(t), transform_error_string(t));
	}

	return (ARCHIVE_FATAL);
}

#if defined(HAVE_MKSTEMP)

/*
 * We can use mkstemp().
 */

int
__archive_mktemp(const char *tmpdir)
{
	struct archive_string temp_name;
	int fd = -1;

	archive_string_init(&temp_name);
	if (tmpdir == NULL) {
		if (get_tempdir(&temp_name) != ARCHIVE_OK)
			goto exit_tmpfile;
	} else {
		archive_strcpy(&temp_name, tmpdir);
		if (temp_name.s[temp_name.length-1] != '/')
			archive_strappend_char(&temp_name, '/');
	}
	archive_strcat(&temp_name, "libarchive_XXXXXX");
	fd = mkstemp(temp_name.s);
	if (fd < 0)
		goto exit_tmpfile;
	unlink(temp_name.s);
exit_tmpfile:
	archive_string_free(&temp_name);
	return (fd);
}

#else

/*
 * We use a private routine.
 */

int
__archive_mktemp(const char *tmpdir)
{
        static const char num[] = {
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
		'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
		'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
		'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
		'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
		'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
		'u', 'v', 'w', 'x', 'y', 'z'
        };
	struct archive_string temp_name;
	struct stat st;
	int fd;
	char *tp, *ep;
	unsigned seed;

	fd = -1;
	archive_string_init(&temp_name);
	if (tmpdir == NULL) {
		if (get_tempdir(&temp_name) != ARCHIVE_OK)
			goto exit_tmpfile;
	} else
		archive_strcpy(&temp_name, tmpdir);
	if (temp_name.s[temp_name.length-1] == '/') {
		temp_name.s[temp_name.length-1] = '\0';
		temp_name.length --;
	}
	if (stat(temp_name.s, &st) < 0)
		goto exit_tmpfile;
	if (!S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		goto exit_tmpfile;
	}
	archive_strcat(&temp_name, "/libarchive_");
	tp = temp_name.s + archive_strlen(&temp_name);
	archive_strcat(&temp_name, "XXXXXXXXXX");
	ep = temp_name.s + archive_strlen(&temp_name);

	fd = open("/dev/random", O_RDONLY);
	if (fd < 0)
		seed = time(NULL);
	else {
		if (read(fd, &seed, sizeof(seed)) < 0)
			seed = time(NULL);
		close(fd);
	}
	do {
		char *p;

		p = tp;
		while (p < ep)
			*p++ = num[((unsigned)rand_r(&seed)) % sizeof(num)];
		fd = open(temp_name.s, O_CREAT | O_EXCL | O_RDWR, 0600);
	} while (fd < 0 && errno == EEXIST);
	if (fd < 0)
		goto exit_tmpfile;
	unlink(temp_name.s);
exit_tmpfile:
	archive_string_free(&temp_name);
	return (fd);
}


#endif /* HAVE_MKSTEMP */
#endif /* !_WIN32 || __CYGWIN__ */
