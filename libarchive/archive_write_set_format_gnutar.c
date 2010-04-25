/*-
 * Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Author: Jonas Gastal <jgastal@profusion.mobi>
 *
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
__FBSDID("$FreeBSD: head/lib/libarchive/archive_write_set_format_gnu_tar.c 191579 2009-04-27 18:35:03Z gastal $");


#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_write_private.h"

struct gnutar {
	uint64_t	entry_bytes_remaining;
	uint64_t	entry_padding;
};

/*
 * Define structure of GNU tar header.
 */
#define	GNUTAR_name_offset 0
#define	GNUTAR_name_size 100
#define	GNUTAR_mode_offset 100
#define	GNUTAR_mode_size 7
#define	GNUTAR_mode_max_size 8
#define	GNUTAR_uid_offset 108
#define	GNUTAR_uid_size 7
#define	GNUTAR_uid_max_size 8
#define	GNUTAR_gid_offset 116
#define	GNUTAR_gid_size 7
#define	GNUTAR_gid_max_size 8
#define	GNUTAR_size_offset 124
#define	GNUTAR_size_size 11
#define	GNUTAR_size_max_size 12
#define	GNUTAR_mtime_offset 136
#define	GNUTAR_mtime_size 11
#define	GNUTAR_mtime_max_size 11
#define	GNUTAR_checksum_offset 148
#define	GNUTAR_checksum_size 8
#define	GNUTAR_typeflag_offset 156
#define	GNUTAR_typeflag_size 1
#define	GNUTAR_linkname_offset 157
#define	GNUTAR_linkname_size 100
#define	GNUTAR_magic_offset 257
#define	GNUTAR_magic_size 6
#define	GNUTAR_version_offset 263
#define	GNUTAR_version_size 2
#define	GNUTAR_uname_offset 265
#define	GNUTAR_uname_size 32
#define	GNUTAR_gname_offset 297
#define	GNUTAR_gname_size 32
#define	GNUTAR_rdevmajor_offset 329
#define	GNUTAR_rdevmajor_size 6
#define	GNUTAR_rdevmajor_max_size 8
#define	GNUTAR_rdevminor_offset 337
#define	GNUTAR_rdevminor_size 6
#define	GNUTAR_rdevminor_max_size 8

/*
 * A filled-in copy of the header for initialization.
 */
static const char template_header[] = {
	/* name: 100 bytes */
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,
	/* Mode, null termination: 8 bytes */
	'0','0','0','0','0','0', '0','\0',
	/* uid, null termination: 8 bytes */
	'0','0','0','0','0','0', '0','\0',
	/* gid, null termination: 8 bytes */
	'0','0','0','0','0','0', '0','\0',
	/* size, space termation: 12 bytes */
	'0','0','0','0','0','0','0','0','0','0','0', '\0',
	/* mtime, space termation: 12 bytes */
	'0','0','0','0','0','0','0','0','0','0','0', '\0',
	/* Initial checksum value: 8 spaces */
	' ',' ',' ',' ',' ',' ',' ',' ',
	/* Typeflag: 1 byte */
	'0',			/* '0' = regular file */
	/* Linkname: 100 bytes */
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,
	/* Magic: 8 bytes */
	'u','s','t','a','r',' ', ' ','\0',
	/* Uname: 32 bytes */
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	/* Gname: 32 bytes */
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	/* rdevmajor + null padding: 8 bytes */
	'\0','\0','\0','\0','\0','\0', '\0','\0',
	/* rdevminor + null padding: 8 bytes */
	'\0','\0','\0','\0','\0','\0', '\0','\0',
	/* Padding: 167 bytes */
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0
};

static int	archive_format_gnutar_header(struct archive_write *, char h[512],
		    struct archive_entry *, int tartype);
static int      archive_write_gnutar_header(struct archive_write *,
		    struct archive_entry *entry);
static ssize_t	archive_write_gnutar_data(struct archive_write *a, const void *buff,
		    size_t s);
static int	archive_write_gnutar_free(struct archive_write *);
static int	archive_write_gnutar_close(struct archive_write *);
static int	archive_write_gnutar_finish_entry(struct archive_write *);
static int	format_256(int64_t, char *, int);
static int	format_number(int64_t, char *, int size, int maxsize);
static int	format_octal(int64_t, char *, int);

/*
 * Set output format to 'GNU tar' format.
 */
int
archive_write_set_format_gnutar(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct gnutar *gnutar;

	gnutar = (struct gnutar *)calloc(1, sizeof(*gnutar));
	if (gnutar == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Can't allocate gnutar data");
		return (ARCHIVE_FATAL);
	}
	a->format_data = gnutar;
	a->format_name = "GNU tar";
	a->format_write_header = archive_write_gnutar_header;
	a->format_write_data = archive_write_gnutar_data;
	a->format_close = archive_write_gnutar_close;
	a->format_free = archive_write_gnutar_free;
	a->format_finish_entry = archive_write_gnutar_finish_entry;
	a->archive.archive_format = ARCHIVE_FORMAT_TAR_GNUTAR;
	a->archive.archive_format_name = "GNU tar";
	return (ARCHIVE_OK);
}

static int
archive_write_gnutar_close(struct archive_write *a)
{
	return (__archive_write_nulls(a, 512*2));
}

static int
archive_write_gnutar_free(struct archive_write *a)
{
	struct gnutar *gnutar;

	gnutar = (struct gnutar *)a->format_data;
	free(gnutar);
	a->format_data = NULL;
	return (ARCHIVE_OK);
}

static int
archive_write_gnutar_finish_entry(struct archive_write *a)
{
	struct gnutar *gnutar;
	int ret;

	gnutar = (struct gnutar *)a->format_data;
	ret = __archive_write_nulls(a,
	    gnutar->entry_bytes_remaining + gnutar->entry_padding);
	gnutar->entry_bytes_remaining = gnutar->entry_padding = 0;
	return (ret);
}

static ssize_t
archive_write_gnutar_data(struct archive_write *a, const void *buff, size_t s)
{
	struct gnutar *gnutar;
	int ret;

	gnutar = (struct gnutar *)a->format_data;
	if (s > gnutar->entry_bytes_remaining)
		s = gnutar->entry_bytes_remaining;
	ret = __archive_write_output(a, buff, s);
	gnutar->entry_bytes_remaining -= s;
	if (ret != ARCHIVE_OK)
		return (ret);
	return (s);
}

static int
archive_write_gnutar_header(struct archive_write *a, struct archive_entry *entry)
{
	char buff[512];
	int ret, ret2;
	int tartype;
	const char *linkname;
	struct gnutar *gnutar;

	gnutar = (struct gnutar *)a->format_data;

	/* Only regular files (not hardlinks) have data. */
	if (archive_entry_hardlink(entry) != NULL ||
	    archive_entry_symlink(entry) != NULL ||
	    !(archive_entry_filetype(entry) == AE_IFREG))
		archive_entry_set_size(entry, 0);

	if (AE_IFDIR == archive_entry_filetype(entry)) {
		const char *p;
		char *t;
		/*
		 * Ensure a trailing '/'.  Modify the entry so
		 * the client sees the change.
		 */
		p = archive_entry_pathname(entry);
		if (p[strlen(p) - 1] != '/') {
			t = (char *)malloc(strlen(p) + 2);
			if (t == NULL) {
				archive_set_error(&a->archive, ENOMEM,
				"Can't allocate gnutar data");
				return(ARCHIVE_FATAL);
			}
			strcpy(t, p);
			strcat(t, "/");
			archive_entry_copy_pathname(entry, t);
			free(t);
		}
	}

	/* If linkname is longer than 100 chars we need to add a 'K' header. */
	linkname = archive_entry_hardlink(entry);
	if (linkname == NULL)
		linkname = archive_entry_symlink(entry);
	if (linkname != NULL && strlen(linkname) > GNUTAR_linkname_size) {
		size_t todo = strlen(linkname);
		struct archive_entry *temp = archive_entry_new();

		/* Uname/gname here don't really matter since noone reads them;
		 * these are the values that GNU tar happens to use on FreeBSD. */
		archive_entry_set_uname(temp, "root");
		archive_entry_set_gname(temp, "wheel");

		archive_entry_set_pathname(temp, "././@LongLink");
		archive_entry_set_size(temp, strlen(linkname) + 1);
		ret = archive_format_gnutar_header(a, buff, temp, 'K');
		if (ret < ARCHIVE_WARN)
			return (ret);
		ret = __archive_write_output(a, buff, 512);
		if(ret < ARCHIVE_WARN)
			return (ret);
		archive_entry_free(temp);
		/* Write as many 512 bytes blocks as needed to write full name. */
		ret = __archive_write_output(a, linkname, todo);
		if(ret < ARCHIVE_WARN)
			return (ret);
		ret = __archive_write_nulls(a, 0x1ff & (-todo));
		if (ret < ARCHIVE_WARN)
			return (ret);
	}

	/* If pathname is longer than 100 chars we need to add an 'L' header. */
	if (strlen(archive_entry_pathname(entry)) > GNUTAR_name_size) {
		const char *pathname = archive_entry_pathname(entry);
		size_t todo = strlen(pathname);
		struct archive_entry *temp = archive_entry_new();

		/* Uname/gname here don't really matter since noone reads them;
		 * these are the values that GNU tar happens to use on FreeBSD. */
		archive_entry_set_uname(temp, "root");
		archive_entry_set_gname(temp, "wheel");

		archive_entry_set_pathname(temp, "././@LongLink");
		archive_entry_set_size(temp, strlen(archive_entry_pathname(entry)) + 1);
		ret = archive_format_gnutar_header(a, buff, temp, 'L');
		if (ret < ARCHIVE_WARN)
			return (ret);
		ret = __archive_write_output(a, buff, 512);
		if(ret < ARCHIVE_WARN)
			return (ret);
		archive_entry_free(temp);
		/* Write as many 512 bytes blocks as needed to write full name. */
		ret = __archive_write_output(a, pathname, todo);
		if(ret < ARCHIVE_WARN)
			return (ret);
		ret = __archive_write_nulls(a, 0x1ff & (-todo));
		if (ret < ARCHIVE_WARN)
			return (ret);
	}

	if (archive_entry_hardlink(entry) != NULL) {
		tartype = '1';
	} else
		switch (archive_entry_filetype(entry)) {
		case AE_IFREG: tartype = '0' ; break;
		case AE_IFLNK: tartype = '2' ; break;
		case AE_IFCHR: tartype = '3' ; break;
		case AE_IFBLK: tartype = '4' ; break;
		case AE_IFDIR: tartype = '5' ; break;
		case AE_IFIFO: tartype = '6' ; break;
		case AE_IFSOCK:
			archive_set_error(&a->archive,
			    ARCHIVE_ERRNO_FILE_FORMAT,
			    "tar format cannot archive socket");
			return (ARCHIVE_FAILED);
		default:
			archive_set_error(&a->archive,
			    ARCHIVE_ERRNO_FILE_FORMAT,
			    "tar format cannot archive this (mode=0%lo)",
			    (unsigned long)archive_entry_mode(entry));
			return (ARCHIVE_FAILED);
		}

	ret = archive_format_gnutar_header(a, buff, entry, tartype);
	if (ret < ARCHIVE_WARN)
		return (ret);
	ret2 = __archive_write_output(a, buff, 512);
	if (ret2 < ARCHIVE_WARN)
		return (ret2);
	if (ret2 < ret)
		ret = ret2;

	gnutar->entry_bytes_remaining = archive_entry_size(entry);
	gnutar->entry_padding = 0x1ff & (-(int64_t)gnutar->entry_bytes_remaining);
	return (ret);
}

static int
archive_format_gnutar_header(struct archive_write *a, char h[512],
    struct archive_entry *entry, int tartype)
{
	unsigned int checksum;
	int i, ret;
	size_t copy_length;
	const char *p;

	ret = 0;

	/*
	 * The "template header" already includes the signature,
	 * various end-of-field markers, and other required elements.
	 */
	memcpy(h, &template_header, 512);

	/*
	 * Because the block is already null-filled, and strings
	 * are allowed to exactly fill their destination (without null),
	 * I use memcpy(dest, src, strlen()) here a lot to copy strings.
	 */

	p = archive_entry_pathname(entry);
	copy_length = strlen(p);
	if (copy_length > GNUTAR_name_size)
		copy_length = GNUTAR_name_size;
	memcpy(h + GNUTAR_name_offset, p, copy_length);

	p = archive_entry_hardlink(entry);
	if (p == NULL)
		p = archive_entry_symlink(entry);
	if (p != NULL && p[0] != '\0') {
		copy_length = strlen(p);
		if (copy_length > GNUTAR_linkname_size)
			copy_length = GNUTAR_linkname_size;
		memcpy(h + GNUTAR_linkname_offset, p, copy_length);
	}

	/* TODO: How does GNU tar handle unames longer than GNUTAR_uname_size? */
	p = archive_entry_uname(entry);
	if (p != NULL && p[0] != '\0') {
		copy_length = strlen(p);
		if (copy_length > GNUTAR_uname_size)
			copy_length = GNUTAR_uname_size;
		memcpy(h + GNUTAR_uname_offset, p, copy_length);
	}

	/* TODO: How does GNU tar handle gnames longer than GNUTAR_gname_size? */
	p = archive_entry_gname(entry);
	if (p != NULL && p[0] != '\0') {
		copy_length = strlen(p);
		if (strlen(p) > GNUTAR_gname_size)
			copy_length = GNUTAR_gname_size;
		memcpy(h + GNUTAR_gname_offset, p, copy_length);
	}

	/* By truncating the mode here, we ensure it always fits. */
	format_octal(archive_entry_mode(entry) & 07777,
	    h + GNUTAR_mode_offset, GNUTAR_mode_size);

	/* TODO: How does GNU tar handle large UIDs? */
	if (format_octal(archive_entry_uid(entry), h + GNUTAR_uid_offset, GNUTAR_uid_size)) {
		archive_set_error(&a->archive, ERANGE, "Numeric user ID %d too large",
		    archive_entry_uid(entry));
		ret = ARCHIVE_FAILED;
	}

	/* TODO: How does GNU tar handle large GIDs? */
	if (format_octal(archive_entry_gid(entry), h + GNUTAR_gid_offset, GNUTAR_gid_size)) {
		archive_set_error(&a->archive, ERANGE, "Numeric group ID %d too large",
		    archive_entry_gid(entry));
		ret = ARCHIVE_FAILED;
	}

	/* GNU tar supports base-256 here, so should never overflow. */
	if (format_number(archive_entry_size(entry), h + GNUTAR_size_offset,
		GNUTAR_size_size, GNUTAR_size_max_size)) {
		archive_set_error(&a->archive, ERANGE, "File size out of range");
		ret = ARCHIVE_FAILED;
	}

	/* Shouldn't overflow before 2106, since mtime field is 33 bits. */
	format_octal(archive_entry_mtime(entry), h + GNUTAR_mtime_offset, GNUTAR_mtime_size);

	if (archive_entry_filetype(entry) == AE_IFBLK
	    || archive_entry_filetype(entry) == AE_IFCHR) {
		if (format_octal(archive_entry_rdevmajor(entry), h + GNUTAR_rdevmajor_offset,
			GNUTAR_rdevmajor_size)) {
			archive_set_error(&a->archive, ERANGE,
			    "Major device number too large");
			ret = ARCHIVE_FAILED;
		}

		if (format_octal(archive_entry_rdevminor(entry), h + GNUTAR_rdevminor_offset,
			GNUTAR_rdevminor_size)) {
			archive_set_error(&a->archive, ERANGE,
			    "Minor device number too large");
			ret = ARCHIVE_FAILED;
		}
	}

	h[GNUTAR_typeflag_offset] = tartype;

	checksum = 0;
	for (i = 0; i < 512; i++)
		checksum += 255 & (unsigned int)h[i];
	h[GNUTAR_checksum_offset + 6] = '\0'; /* Can't be pre-set in the template. */
	/* h[GNUTAR_checksum_offset + 7] = ' '; */ /* This is pre-set in the template. */
	format_octal(checksum, h + GNUTAR_checksum_offset, 6);
	return (ret);
}

/*
 * Format a number into a field, falling back to base-256 if necessary.
 */
static int
format_number(int64_t v, char *p, int s, int maxsize)
{
	int64_t limit = ((int64_t)1 << (s*3));

	if (v < limit)
		return (format_octal(v, p, s));
	return (format_256(v, p, maxsize));
}

/*
 * Format a number into the specified field using base-256.
 */
static int
format_256(int64_t v, char *p, int s)
{
	p += s;
	while (s-- > 0) {
		*--p = (char)(v & 0xff);
		v >>= 8;
	}
	*p |= 0x80; /* Set the base-256 marker bit. */
	return (0);
}

/*
 * Format a number into the specified field using octal.
 */
static int
format_octal(int64_t v, char *p, int s)
{
	int len = s;

	/* Octal values can't be negative, so use 0. */
	if (v < 0)
		v = 0;

	p += s;		/* Start at the end and work backwards. */
	while (s-- > 0) {
		*--p = (char)('0' + (v & 7));
		v >>= 3;
	}

	if (v == 0)
		return (0);

	/* If it overflowed, fill field with max value. */
	while (len-- > 0)
		*p++ = '7';

	return (-1);
}
