/*-
 * Copyright (c) 2004-2010 Tim Kientzle
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
__FBSDID("$FreeBSD$");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <time.h>
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_private.h"
#include "archive_endian.h"

#ifndef HAVE_ZLIB_H
#include "archive_crc32.h"
#endif

struct zip {
	/* entry_bytes_remaining is the number of bytes we expect. */
	int64_t			entry_bytes_remaining;
	int64_t			entry_offset;

	/* These count the number of bytes actually read for the entry. */
	int64_t			entry_compressed_bytes_read;
	int64_t			entry_uncompressed_bytes_read;

	/* Running CRC32 of the decompressed data */
	unsigned long		entry_crc32;

	unsigned		version;
	unsigned		system;
	unsigned		flags;
	unsigned		compression;
	const char *		compression_name;
	mode_t			mode;

	/* Flags to mark progress of decompression. */
	char			decompress_init;
	char			end_of_entry;

	unsigned long		crc32;
	ssize_t			filename_length;
	int64_t			uncompressed_size;
	int64_t			compressed_size;

	unsigned char 		*uncompressed_buffer;
	size_t 			uncompressed_buffer_size;
#ifdef HAVE_ZLIB_H
	z_stream		stream;
	char			stream_valid;
#endif

	char			looked_for_central_directory;
	char			have_central_directory;

	struct archive_string	pathname;
	char	format_name[64];
};

#define ZIP_LENGTH_AT_END	8

struct zip_file_header {
	char	signature[4];
	char	version[2];
	char	flags[2];
	char	compression[2];
	char	timedate[4];
	char	crc32[4];
	char	compressed_size[4];
	char	uncompressed_size[4];
	char	filename_length[2];
	char	extra_length[2];
};

static const char *compression_names[] = {
	"uncompressed",
	"shrinking",
	"reduced-1",
	"reduced-2",
	"reduced-3",
	"reduced-4",
	"imploded",
	"reserved",
	"deflation"
};


static int	archive_read_format_zip_bid(struct archive_read *);
static int	archive_read_format_zip_cleanup(struct archive_read *);
#if ARCHIVE_VERSION_NUMBER < 3000000
static int	archive_read_format_zip_read_data(struct archive_read *,
		    const void **, size_t *, off_t *);
#else
static int	archive_read_format_zip_read_data(struct archive_read *,
		    const void **, size_t *, int64_t *);
#endif
static int	archive_read_format_zip_read_data_skip(struct archive_read *a);
static int	archive_read_format_zip_read_header(struct archive_read *,
		    struct archive_entry *);
static int	bid_with_seek(struct archive_read *, int64_t);
static int	bid_without_seek(struct archive_read *);
static int	find_central_directory(struct archive_read *, int64_t);
static int	read_central_directory(struct archive_read *, struct zip *);
#if ARCHIVE_VERSION_NUMBER < 3000000
static int	zip_read_data_deflate(struct archive_read *a, const void **buff,
		    size_t *size, off_t *offset);
static int	zip_read_data_none(struct archive_read *a, const void **buff,
		    size_t *size, off_t *offset);
#else
static int	zip_read_data_deflate(struct archive_read *a, const void **buff,
		    size_t *size, int64_t *offset);
static int	zip_read_data_none(struct archive_read *a, const void **buff,
		    size_t *size, int64_t *offset);
#endif
static int	zip_read_local_file_header(struct archive_read *a,
		    struct archive_entry *entry, struct zip *zip);
static time_t	zip_time(const char *);
static int	process_extra(struct archive_read *, struct archive_entry *,
		    struct zip *, size_t);

int
archive_read_support_format_zip(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct zip *zip;
	int r;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_read_support_format_zip");

	zip = (struct zip *)malloc(sizeof(*zip));
	if (zip == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Can't allocate zip data");
		return (ARCHIVE_FATAL);
	}
	memset(zip, 0, sizeof(*zip));

	r = __archive_read_register_format(a,
	    zip,
	    "zip",
	    archive_read_format_zip_bid,
	    NULL,
	    archive_read_format_zip_read_header,
	    archive_read_format_zip_read_data,
	    archive_read_format_zip_read_data_skip,
	    archive_read_format_zip_cleanup);

	if (r != ARCHIVE_OK)
		free(zip);
	return (ARCHIVE_OK);
}

static int
archive_read_format_zip_bid(struct archive_read *a)
{
	int64_t file_size = __archive_read_seek(a, 0, SEEK_END);
	int bid = 0;

	if (file_size < 0)
		return (bid_without_seek(a));
	bid = bid_with_seek(a, file_size);
	__archive_read_seek(a, 0, SEEK_SET);
	return (bid);
}

/*
 * If we can seek, then we bid by just trying to look up the
 * Central Directory.
 */
static int
bid_with_seek(struct archive_read *a, int64_t file_size)
{
	int64_t toc_entries = find_central_directory(a, file_size);
	if (toc_entries > 0)
		return 64;
	return (0);
}

/*
 * Locate the Central Directory.
 * If we can seek the input, we use this both for bidding and
 * to read the Central Directory before pulling files.
 *
 * Returns the number of entries in the central directory or -1 if the
 * central directory can't be found.  If the central directory was
 * found, then the current file pointer is at the beginning of the
 * central directory.
 */
static int
find_central_directory(struct archive_read *a, int64_t file_size)
{
	int64_t block_end = file_size;
	/* TODO:  Read the Zip spec again and replace this comment
	 * with an explanation why 64k is enough. */
	int64_t block_start = file_size - 64 * 1024;
	ssize_t avail;
	const char *start, *end, *p;

	/* XXX Need a better value for minimum size of a Zip archive. */
	if (file_size < 32)
		return (-1);

	if (block_start < 0)
		block_start = 0;
	if (block_start != __archive_read_seek(a, block_start, SEEK_SET))
		return (-1);
	start = __archive_read_ahead(a, block_end - block_start, &avail);
	if (start == NULL)
		return (-1);

	end = start + (size_t)(block_end - block_start);
	for (p = start; p < end; ++p) {
		int64_t end_start, toc_size, toc_start, comment_length;
		int disk_number, disk_with_toc, toc_entries_on_disk, toc_entries, toc_entries_disk;

		/* End of Central Directory signature */
		if (p[0] != 'P' || p[1] != 'K'
		    || p[2] != '\005' || p[3] != '\006')
			continue;
		/* End of Central Dir record is 22 bytes long. */
		if ((end - p) < 22)
			continue;

		/* Start of the PK56 end-of-central-directory record. */
		end_start = block_end - (end - p);
		comment_length = archive_le16dec(p + 20);

		/* End-of-archive comment can't extend past end of file. */
		/* XXX Can there be garbage after end-of-central-dir
		 * record? XXX */
		if ((end_start + comment_length + 22) != file_size)
			continue;

		/* Position of central directory. */
		toc_size = archive_le32dec(p + 12);
		toc_start = archive_le32dec(p + 16);

		/* XXX Can there be garbage between end of central dir
		 * and start of end-of-central dir record?  XXX */
		if (toc_size + toc_start != end_start)
			continue;

		disk_number = archive_le16dec(p + 4);
		disk_with_toc = archive_le16dec(p + 6);
		toc_entries_on_disk = archive_le16dec(p + 8);
		toc_entries = archive_le16dec(p + 10);

		/* Each central dir record is at least 46 bytes. */
		if (toc_size < 46 * toc_entries)
			continue;

		/* Failed checks before this point indicate that the
		 * candidate end-of-central-dir record is invalid and
		 * should be ignored. */

		/* Failed checks after this point indicate that this
		 * is a format we don't support.  In particular, we
		 * don't support spanned or split archives. */
		if (disk_number != 0 || disk_with_toc != 0)
			return (-1);
		if (toc_entries_on_disk != toc_entries)
			return (-1);

		/* The following seek will cause the cached block
		 * we've been scanning to be dropped.  That's why
		 * after this point we must return(-1) instead of trying
		 * to continue the outer scan. */
		if (toc_start != __archive_read_seek(a, toc_start, SEEK_SET))
			return (-1);

		/* Final check: Follow the offset to the Central Dir
		 * and check that there's a Central Dir record there. */
		p = __archive_read_ahead(a, 4, &avail);
		if (p == NULL)
			return (-1);
		if (p[0] != 'P' || p[1] != 'K'
		    || p[2] != '\001' || p[3] != '\002')
			return (-1);

		return (toc_entries_disk);
	}
	return (-1);
}

/*
 * If we can seek, then we bid by just trying to look up the
 * Central Directory.
 */
static int
read_central_directory(struct archive_read *a, struct zip *zip)
{
	int64_t file_size;
	int64_t toc_entries;
	//const char *p;

	file_size = __archive_read_seek(a, 0, SEEK_END);
	if (file_size < 0)
		return ARCHIVE_FAILED;
	toc_entries = find_central_directory(a, file_size);
	if (toc_entries < 0)
		return ARCHIVE_FAILED;

/*
	for (;;) {
		p = __archive_read_ahead(a, 4, &avail);
		if (p[0] != 'P' || p[1] != 'K'
	}
*/
	return (ARCHIVE_OK);
}


/*
 * Bid without seeking when reading from stdin, sockets, tape drives,
 * or other non-seekable media.  Fortunately, ZIP format has just
 * enough information in the file headers to make a straight streaming
 * read possible, though there are a few ZIP features that we can't
 * really support in this mode.
 */
static int
bid_without_seek(struct archive_read *a)
{
	const char *p;

	if ((p = __archive_read_ahead(a, 4, NULL)) == NULL)
		return (-1);

	/*
	 * Bid of 30 here is: 16 bits for "PK",
	 * next 16-bit field has four options (-2 bits).
	 * 16 + 16-2 = 30.
	 */
	if (p[0] == 'P' && p[1] == 'K') {
		if ((p[2] == '\001' && p[3] == '\002')
		    || (p[2] == '\003' && p[3] == '\004')
		    || (p[2] == '\005' && p[3] == '\006')
		    || (p[2] == '\007' && p[3] == '\010')
		    || (p[2] == '0' && p[3] == '0'))
			return (30);
	}

	return (0);
}

static int
archive_read_format_zip_read_header(struct archive_read *a,
    struct archive_entry *entry)
{
	const void *h;
	const char *signature;
	struct zip *zip = (struct zip *)(a->format->data);
	int r = ARCHIVE_OK, r1;

	a->archive.archive_format = ARCHIVE_FORMAT_ZIP;
	if (a->archive.archive_format_name == NULL)
		a->archive.archive_format_name = "ZIP";

	if (!zip->looked_for_central_directory) {
		zip->looked_for_central_directory = 1;
		if (read_central_directory(a, zip) == ARCHIVE_OK)
			zip->have_central_directory = 1;
		/* XXX */ __archive_read_seek(a, 0, SEEK_SET);
	}
	/* XXX If we have central dir in memory, identify next
	 * file and seek to it. */

	zip->decompress_init = 0;
	zip->end_of_entry = 0;
	zip->entry_uncompressed_bytes_read = 0;
	zip->entry_compressed_bytes_read = 0;
	zip->entry_crc32 = crc32(0, NULL, 0);
	if ((h = __archive_read_ahead(a, 4, NULL)) == NULL)
		return (ARCHIVE_FATAL);

	signature = (const char *)h;
	if (signature[0] != 'P' || signature[1] != 'K') {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Bad ZIP file");
		return (ARCHIVE_FATAL);
	}

	/*
	 * "PK00" signature is used for "split" archives that
	 * only have a single segment.  This means we can just
	 * skip the PK00; the first real file header should follow.
	 */
	if (signature[2] == '0' && signature[3] == '0') {
		__archive_read_consume(a, 4);
		if ((h = __archive_read_ahead(a, 4, NULL)) == NULL)
			return (ARCHIVE_FATAL);
		signature = (const char *)h;
		if (signature[0] != 'P' || signature[1] != 'K') {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
			    "Bad ZIP file");
			return (ARCHIVE_FATAL);
		}
	}

	if (signature[2] == '\001' && signature[3] == '\002') {
		/* Beginning of central directory. */
		return (ARCHIVE_EOF);
	}

	if (signature[2] == '\003' && signature[3] == '\004') {
		/* Regular file entry. */
		r1 = zip_read_local_file_header(a, entry, zip);
		if (r1 != ARCHIVE_OK)
			return (r1);
		return (r);
	}

	if (signature[2] == '\005' && signature[3] == '\006') {
		/* End-of-archive record. */
		return (ARCHIVE_EOF);
	}

	if (signature[2] == '\007' && signature[3] == '\010') {
		/*
		 * We should never encounter this record here;
		 * see ZIP_LENGTH_AT_END handling below for details.
		 */
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Bad ZIP file: Unexpected end-of-entry record");
		return (ARCHIVE_FATAL);
	}

	archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
	    "Damaged ZIP file or unsupported format variant (%d,%d)",
	    signature[2], signature[3]);
	return (ARCHIVE_FATAL);
}

static int
zip_read_file_header(struct archive_read *a,
    struct archive_entry *entry, struct zip *zip)
{
	const struct zip_file_header *p;
	const void *h;
	int r;
	size_t extra_length;

	if ((p = __archive_read_ahead(a, sizeof *p, NULL)) == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated ZIP file header");
		return (ARCHIVE_FATAL);
	}

	zip->version = p->version[0];
	zip->system = p->version[1];
	zip->flags = archive_le16dec(p->flags);
	zip->compression = archive_le16dec(p->compression);
	if (zip->compression <
	    sizeof(compression_names)/sizeof(compression_names[0]))
		zip->compression_name = compression_names[zip->compression];
	else
		zip->compression_name = "??";
	archive_entry_set_mtime(entry, zip_time(p->timedate), 0);
	zip->mode = 0;
	zip->crc32 = archive_le32dec(p->crc32);
	zip->filename_length = archive_le16dec(p->filename_length);
	extra_length = archive_le16dec(p->extra_length);
	zip->uncompressed_size = archive_le32dec(p->uncompressed_size);
	zip->compressed_size = archive_le32dec(p->compressed_size);

	__archive_read_consume(a, sizeof(struct zip_file_header));


	/* Read the filename. */
	if ((h = __archive_read_ahead(a, zip->filename_length, NULL)) == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated ZIP file header");
		return (ARCHIVE_FATAL);
	}
	if (archive_string_ensure(&zip->pathname, zip->filename_length) == NULL)
		__archive_errx(1, "Out of memory");
	archive_strncpy(&zip->pathname, h, zip->filename_length);
	__archive_read_consume(a, zip->filename_length);
	archive_entry_set_pathname(entry, zip->pathname.s);

	if (zip->pathname.s[archive_strlen(&zip->pathname) - 1] == '/')
		zip->mode = AE_IFDIR | 0777;
	else
		zip->mode = AE_IFREG | 0777;

	/* Read the extra data. */
	r = process_extra(a, entry, zip, extra_length);
	if (r != ARCHIVE_OK)
		return (r);

	/* Populate some additional entry fields: */
	archive_entry_set_mode(entry, zip->mode);
	/* Set the size only if it's meaningful. */
	if (0 == (zip->flags & ZIP_LENGTH_AT_END))
		archive_entry_set_size(entry, zip->uncompressed_size);

	zip->entry_bytes_remaining = zip->compressed_size;
	zip->entry_offset = 0;

	/* If there's no body, force read_data() to return EOF immediately. */
	if (0 == (zip->flags & ZIP_LENGTH_AT_END)
	    && zip->entry_bytes_remaining < 1)
		zip->end_of_entry = 1;

	/* Set up a more descriptive format name. */
	sprintf(zip->format_name, "ZIP %d.%d (%s)",
	    zip->version / 10, zip->version % 10,
	    zip->compression_name);
	a->archive.archive_format_name = zip->format_name;

	return (ARCHIVE_OK);
}

static int
zip_read_local_file_header(struct archive_read *a,
    struct archive_entry *entry, struct zip *zip)
{
	const struct zip_file_header *p;
	const void *h;
	int r;
	size_t extra_length;

	if ((p = __archive_read_ahead(a, sizeof *p, NULL)) == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated ZIP file header");
		return (ARCHIVE_FATAL);
	}

	zip->version = p->version[0];
	zip->system = p->version[1];
	zip->flags = archive_le16dec(p->flags);
	zip->compression = archive_le16dec(p->compression);
	if (zip->compression <
	    sizeof(compression_names)/sizeof(compression_names[0]))
		zip->compression_name = compression_names[zip->compression];
	else
		zip->compression_name = "??";
	archive_entry_set_mtime(entry, zip_time(p->timedate), 0);
	zip->mode = 0;
	zip->crc32 = archive_le32dec(p->crc32);
	zip->filename_length = archive_le16dec(p->filename_length);
	extra_length = archive_le16dec(p->extra_length);
	zip->uncompressed_size = archive_le32dec(p->uncompressed_size);
	zip->compressed_size = archive_le32dec(p->compressed_size);

	__archive_read_consume(a, sizeof(struct zip_file_header));


	/* Read the filename. */
	if ((h = __archive_read_ahead(a, zip->filename_length, NULL)) == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated ZIP file header");
		return (ARCHIVE_FATAL);
	}
	if (archive_string_ensure(&zip->pathname, zip->filename_length) == NULL)
		__archive_errx(1, "Out of memory");
	archive_strncpy(&zip->pathname, h, zip->filename_length);
	__archive_read_consume(a, zip->filename_length);
	archive_entry_set_pathname(entry, zip->pathname.s);

	if (zip->pathname.s[archive_strlen(&zip->pathname) - 1] == '/')
		zip->mode = AE_IFDIR | 0777;
	else
		zip->mode = AE_IFREG | 0777;

	/* Read the extra data. */
	r = process_extra(a, entry, zip, extra_length);
	if (r != ARCHIVE_OK)
		return (r);

	/* Populate some additional entry fields: */
	archive_entry_set_mode(entry, zip->mode);
	/* Set the size only if it's meaningful. */
	if (0 == (zip->flags & ZIP_LENGTH_AT_END))
		archive_entry_set_size(entry, zip->uncompressed_size);

	zip->entry_bytes_remaining = zip->compressed_size;
	zip->entry_offset = 0;

	/* If there's no body, force read_data() to return EOF immediately. */
	if (0 == (zip->flags & ZIP_LENGTH_AT_END)
	    && zip->entry_bytes_remaining < 1)
		zip->end_of_entry = 1;

	/* Set up a more descriptive format name. */
	sprintf(zip->format_name, "ZIP %d.%d (%s)",
	    zip->version / 10, zip->version % 10,
	    zip->compression_name);
	a->archive.archive_format_name = zip->format_name;

	return (ARCHIVE_OK);
}

/* Convert an MSDOS-style date/time into Unix-style time. */
static time_t
zip_time(const char *p)
{
	int msTime, msDate;
	struct tm ts;

	msTime = (0xff & (unsigned)p[0]) + 256 * (0xff & (unsigned)p[1]);
	msDate = (0xff & (unsigned)p[2]) + 256 * (0xff & (unsigned)p[3]);

	memset(&ts, 0, sizeof(ts));
	ts.tm_year = ((msDate >> 9) & 0x7f) + 80; /* Years since 1900. */
	ts.tm_mon = ((msDate >> 5) & 0x0f) - 1; /* Month number. */
	ts.tm_mday = msDate & 0x1f; /* Day of month. */
	ts.tm_hour = (msTime >> 11) & 0x1f;
	ts.tm_min = (msTime >> 5) & 0x3f;
	ts.tm_sec = (msTime << 1) & 0x3e;
	ts.tm_isdst = -1;
	return mktime(&ts);
}

#if ARCHIVE_VERSION_NUMBER < 3000000
static int
archive_read_format_zip_read_data(struct archive_read *a,
    const void **buff, size_t *size, off_t *offset)
#else
static int
archive_read_format_zip_read_data(struct archive_read *a,
    const void **buff, size_t *size, int64_t *offset)
#endif
{
	int r;
	struct zip *zip;

	zip = (struct zip *)(a->format->data);

	/*
	 * If we hit end-of-entry last time, clean up and return
	 * ARCHIVE_EOF this time.
	 */
	if (zip->end_of_entry) {
		*offset = zip->entry_uncompressed_bytes_read;
		*size = 0;
		*buff = NULL;
		return (ARCHIVE_EOF);
	}

	switch(zip->compression) {
	case 0:  /* No compression. */
		r =  zip_read_data_none(a, buff, size, offset);
		break;
	case 8: /* Deflate compression. */
		r =  zip_read_data_deflate(a, buff, size, offset);
		break;
	default: /* Unsupported compression. */
		*buff = NULL;
		*size = 0;
		*offset = 0;
		/* Return a warning. */
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Unsupported ZIP compression method (%s)",
		    zip->compression_name);
		if (zip->flags & ZIP_LENGTH_AT_END) {
			/*
			 * ZIP_LENGTH_AT_END requires us to
			 * decompress the entry in order to
			 * skip it, but we don't know this
			 * compression method, so we give up.
			 */
			r = ARCHIVE_FATAL;
		} else {
			/* We can't decompress this entry, but we will
			 * be able to skip() it and try the next entry. */
			r = ARCHIVE_WARN;
		}
		break;
	}
	if (r != ARCHIVE_OK)
		return (r);
	/* Update checksum */
	if (*size)
		zip->entry_crc32 = crc32(zip->entry_crc32, *buff, *size);
	/* If we hit the end, swallow any end-of-data marker. */
	if (zip->end_of_entry) {
		if (zip->flags & ZIP_LENGTH_AT_END) {
			const char *p;

			if ((p = __archive_read_ahead(a, 16, NULL)) == NULL) {
				archive_set_error(&a->archive,
				    ARCHIVE_ERRNO_FILE_FORMAT,
				    "Truncated ZIP end-of-file record");
				return (ARCHIVE_FATAL);
			}
			zip->crc32 = archive_le32dec(p + 4);
			zip->compressed_size = archive_le32dec(p + 8);
			zip->uncompressed_size = archive_le32dec(p + 12);
			__archive_read_consume(a, 16);
		}
		/* Check file size, CRC against these values. */
		if (zip->compressed_size != zip->entry_compressed_bytes_read) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "ZIP compressed data is wrong size");
			return (ARCHIVE_WARN);
		}
		/* Size field only stores the lower 32 bits of the actual size. */
		if ((zip->uncompressed_size & UINT32_MAX)
		    != (zip->entry_uncompressed_bytes_read & UINT32_MAX)) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "ZIP uncompressed data is wrong size");
			return (ARCHIVE_WARN);
		}
		/* Check computed CRC against header */
		if (zip->crc32 != zip->entry_crc32) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "ZIP bad CRC: 0x%lx should be 0x%lx",
			    zip->entry_crc32, zip->crc32);
			return (ARCHIVE_WARN);
		}
	}

	/* Return EOF immediately if this is a non-regular file. */
	if (AE_IFREG != (zip->mode & AE_IFMT))
		return (ARCHIVE_EOF);
	return (ARCHIVE_OK);
}

/*
 * Read "uncompressed" data.  According to the current specification,
 * if ZIP_LENGTH_AT_END is specified, then the size fields in the
 * initial file header are supposed to be set to zero.  This would, of
 * course, make it impossible for us to read the archive, since we
 * couldn't determine the end of the file data.  Info-ZIP seems to
 * include the real size fields both before and after the data in this
 * case (the CRC only appears afterwards), so this works as you would
 * expect.
 *
 * Returns ARCHIVE_OK if successful, ARCHIVE_FATAL otherwise, sets
 * zip->end_of_entry if it consumes all of the data.
 */
#if ARCHIVE_VERSION_NUMBER < 3000000
static int
zip_read_data_none(struct archive_read *a, const void **buff,
    size_t *size, off_t *offset)
#else
static int
zip_read_data_none(struct archive_read *a, const void **buff,
    size_t *size, int64_t *offset)
#endif
{
	struct zip *zip;
	ssize_t bytes_avail;

	zip = (struct zip *)(a->format->data);

	if (zip->entry_bytes_remaining == 0) {
		*buff = NULL;
		*size = 0;
		*offset = zip->entry_offset;
		zip->end_of_entry = 1;
		return (ARCHIVE_OK);
	}
	/*
	 * Note: '1' here is a performance optimization.
	 * Recall that the decompression layer returns a count of
	 * available bytes; asking for more than that forces the
	 * decompressor to combine reads by copying data.
	 */
	*buff = __archive_read_ahead(a, 1, &bytes_avail);
	if (bytes_avail <= 0) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated ZIP file data");
		return (ARCHIVE_FATAL);
	}
	if (bytes_avail > zip->entry_bytes_remaining)
		bytes_avail = zip->entry_bytes_remaining;
	__archive_read_consume(a, bytes_avail);
	*size = bytes_avail;
	*offset = zip->entry_offset;
	zip->entry_offset += *size;
	zip->entry_bytes_remaining -= *size;
	zip->entry_uncompressed_bytes_read += *size;
	zip->entry_compressed_bytes_read += *size;
	return (ARCHIVE_OK);
}

#ifdef HAVE_ZLIB_H
#if ARCHIVE_VERSION_NUMBER < 3000000
static int
zip_read_data_deflate(struct archive_read *a, const void **buff,
    size_t *size, off_t *offset)
#else
static int
zip_read_data_deflate(struct archive_read *a, const void **buff,
    size_t *size, int64_t *offset)
#endif
{
	struct zip *zip;
	ssize_t bytes_avail;
	const void *compressed_buff;
	int r;

	zip = (struct zip *)(a->format->data);

	/* If the buffer hasn't been allocated, allocate it now. */
	if (zip->uncompressed_buffer == NULL) {
		zip->uncompressed_buffer_size = 32 * 1024;
		zip->uncompressed_buffer
		    = (unsigned char *)malloc(zip->uncompressed_buffer_size);
		if (zip->uncompressed_buffer == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "No memory for ZIP decompression");
			return (ARCHIVE_FATAL);
		}
	}

	/* If we haven't yet read any data, initialize the decompressor. */
	if (!zip->decompress_init) {
		if (zip->stream_valid)
			r = inflateReset(&zip->stream);
		else
			r = inflateInit2(&zip->stream,
			    -15 /* Don't check for zlib header */);
		if (r != Z_OK) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Can't initialize ZIP decompression.");
			return (ARCHIVE_FATAL);
		}
		/* Stream structure has been set up. */
		zip->stream_valid = 1;
		/* We've initialized decompression for this stream. */
		zip->decompress_init = 1;
	}

	/*
	 * Note: '1' here is a performance optimization.
	 * Recall that the decompression layer returns a count of
	 * available bytes; asking for more than that forces the
	 * decompressor to combine reads by copying data.
	 */
	compressed_buff = __archive_read_ahead(a, 1, &bytes_avail);
	if (bytes_avail <= 0) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated ZIP file body");
		return (ARCHIVE_FATAL);
	}

	/*
	 * A bug in zlib.h: stream.next_in should be marked 'const'
	 * but isn't (the library never alters data through the
	 * next_in pointer, only reads it).  The result: this ugly
	 * cast to remove 'const'.
	 */
	zip->stream.next_in = (Bytef *)(uintptr_t)(const void *)compressed_buff;
	zip->stream.avail_in = bytes_avail;
	zip->stream.total_in = 0;
	zip->stream.next_out = zip->uncompressed_buffer;
	zip->stream.avail_out = zip->uncompressed_buffer_size;
	zip->stream.total_out = 0;

	r = inflate(&zip->stream, 0);
	switch (r) {
	case Z_OK:
		break;
	case Z_STREAM_END:
		zip->end_of_entry = 1;
		break;
	case Z_MEM_ERROR:
		archive_set_error(&a->archive, ENOMEM,
		    "Out of memory for ZIP decompression");
		return (ARCHIVE_FATAL);
	default:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "ZIP decompression failed (%d)", r);
		return (ARCHIVE_FATAL);
	}

	/* Consume as much as the compressor actually used. */
	bytes_avail = zip->stream.total_in;
	__archive_read_consume(a, bytes_avail);
	zip->entry_bytes_remaining -= bytes_avail;
	zip->entry_compressed_bytes_read += bytes_avail;

	*offset = zip->entry_offset;
	*size = zip->stream.total_out;
	zip->entry_uncompressed_bytes_read += *size;
	*buff = zip->uncompressed_buffer;
	zip->entry_offset += *size;
	return (ARCHIVE_OK);
}
#else
#if ARCHIVE_VERSION_NUMBER < 3000000
static int
zip_read_data_deflate(struct archive_read *a, const void **buff,
    size_t *size, off_t *offset)
#else
static int
zip_read_data_deflate(struct archive_read *a, const void **buff,
    size_t *size, int64_t *offset)
#endif
{
	*buff = NULL;
	*size = 0;
	*offset = 0;
	archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
	    "libarchive compiled without deflate support (no libz)");
	return (ARCHIVE_FATAL);
}
#endif

static int
archive_read_format_zip_read_data_skip(struct archive_read *a)
{
	struct zip *zip;
	const void *buff = NULL;
	off_t bytes_skipped;

	zip = (struct zip *)(a->format->data);

	/* If we've already read to end of data, we're done. */
	if (zip->end_of_entry)
		return (ARCHIVE_OK);

	/*
	 * If the length is at the end, we have no choice but
	 * to decompress all the data to find the end marker.
	 */
	if (zip->flags & ZIP_LENGTH_AT_END) {
		size_t size;
#if ARCHIVE_VERSION_NUMBER < 3000000
		off_t offset;
#else
		int64_t offset;
#endif
		int r;
		do {
			r = archive_read_format_zip_read_data(a, &buff,
			    &size, &offset);
		} while (r == ARCHIVE_OK);
		return (r);
	}

	/*
	 * If the length is at the beginning, we can skip the
	 * compressed data much more quickly.
	 */
	bytes_skipped = __archive_read_consume(a, zip->entry_bytes_remaining);
	if (bytes_skipped < 0)
		return (ARCHIVE_FATAL);

	/* This entry is finished and done. */
	zip->end_of_entry = 1;
	return (ARCHIVE_OK);
}

static int
archive_read_format_zip_cleanup(struct archive_read *a)
{
	struct zip *zip;

	zip = (struct zip *)(a->format->data);
#ifdef HAVE_ZLIB_H
	if (zip->stream_valid)
		inflateEnd(&zip->stream);
#endif
	free(zip->uncompressed_buffer);
	archive_string_free(&(zip->pathname));
	free(zip);
	(a->format->data) = NULL;
	return (ARCHIVE_OK);
}

/*
 * The extra data is stored as a list of
 *	id1+size1+data1 + id2+size2+data2 ...
 *  triplets.  id and size are 2 bytes each.
 */
static int
process_extra(struct archive_read *a, struct archive_entry *entry,
    struct zip* zip, size_t extra_length)
{
	int offset = 0;
	int64_t r;
	const char *p;

	if ((p = __archive_read_ahead(a, extra_length, NULL)) == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated ZIP header");
		return (ARCHIVE_FATAL);
	}

	while (offset < extra_length - 4)
	{
		unsigned short headerid = archive_le16dec(p + offset);
		unsigned short datasize = archive_le16dec(p + offset + 2);
		offset += 4;
		if (offset + datasize > extra_length)
			break;
#ifdef DEBUG
		fprintf(stderr, "Header id 0x%04x, length %d\n",
		    headerid, datasize);
#endif
		switch (headerid) {
		case 0x0001:
			/* Zip64 extended information extra field. */
			if (datasize >= 8)
				zip->uncompressed_size = archive_le64dec(p + offset);
			if (datasize >= 16)
				zip->compressed_size = archive_le64dec(p + offset + 8);
			break;
		case 0x5455:
		{
			/* Extended time field "UT". */
			int flags = p[offset];
			offset++;
			datasize--;
			/* Flag bits indicate which dates are present. */
			if (flags & 0x01)
			{
#ifdef DEBUG
				fprintf(stderr, "mtime: %lld -> %d\n",
				    (long long)zip->mtime,
				    archive_le32dec(p + offset));
#endif
				if (datasize < 4)
					break;
				archive_entry_set_mtime(entry,
				    archive_le32dec(p + offset), 0);
				offset += 4;
				datasize -= 4;
			}
			if (flags & 0x02)
			{
				if (datasize < 4)
					break;
				archive_entry_set_atime(entry,
				    archive_le32dec(p + offset), 0);
				offset += 4;
				datasize -= 4;
			}
			if (flags & 0x04)
			{
				if (datasize < 4)
					break;
				archive_entry_set_ctime(entry,
				    archive_le32dec(p + offset), 0);
				offset += 4;
				datasize -= 4;
			}
			break;
		}
		case 0x7855:
			/* Info-ZIP Unix Extra Field (type 2) "Ux". */
#ifdef DEBUG
			fprintf(stderr, "uid %d gid %d\n",
			    archive_le16dec(p + offset),
			    archive_le16dec(p + offset + 2));
#endif
			if (datasize >= 2)
				archive_entry_set_uid(entry,
				    archive_le16dec(p + offset));
			if (datasize >= 4)
				archive_entry_set_gid(entry,
				    archive_le16dec(p + offset + 2));
			break;
		default:
			break;
		}
		offset += datasize;
	}
#ifdef DEBUG
	if (offset != extra_length)
	{
		fprintf(stderr,
		    "Extra data field contents do not match reported size!");
	}
#endif

	r = __archive_read_consume(a, extra_length);
	if (r == extra_length)
		return (ARCHIVE_OK);
	return (r);
}
