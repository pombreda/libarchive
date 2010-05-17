/*-
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
 *
 * $FreeBSD: head/lib/libarchive/archive_read_private.h 201088 2009-12-28 02:18:55Z kientzle $
 */

#ifndef __LIBARCHIVE_BUILD
#error This header is only to be used internally to libarchive.
#endif

#ifndef ARCHIVE_READ_PRIVATE_H_INCLUDED
#define	ARCHIVE_READ_PRIVATE_H_INCLUDED

#include <transform.h>
#include "archive.h"
#include "archive_string.h"
#include "archive_private.h"

struct archive_read {
	struct archive	archive;

	struct archive_entry	*entry;

	/* Dev/ino of the archive being read/written. */
	dev_t		  skip_file_dev;
	ino_t		  skip_file_ino;

	/*
	 * Used by archive_read_data() to track blocks and copy
	 * data to client buffers, filling gaps with zero bytes.
	 */
	const char	 *read_data_block;
#if ARCHIVE_VERSION_NUMBER < 3000000
	off_t		  read_data_offset;
#else
	int64_t		  read_data_offset;
#endif
	off_t		  read_data_output_offset;
	size_t		  read_data_remaining;

	/* File offset of beginning of most recently-read header. */
	off_t		  header_position;

	/*
	 * Format detection is mostly the same as compression
	 * detection, with one significant difference: The bidders
	 * use the read_ahead calls above to examine the stream rather
	 * than having the supervisor hand them a block of data to
	 * examine.
	 */

	struct archive_format_descriptor {
		void	 *data;
		const char *name;
		int	(*bid)(struct archive_read *);
		int	(*options)(struct archive_read *, const char *key,
		    const char *value);
		int	(*read_header)(struct archive_read *, struct archive_entry *);
#if ARCHIVE_VERSION_NUMBER < 3000000
		int	(*read_data)(struct archive_read *, const void **, size_t *, off_t *);
#else
		int	(*read_data)(struct archive_read *, const void **, size_t *, int64_t *);
#endif
		int	(*read_data_skip)(struct archive_read *);
		int	(*cleanup)(struct archive_read *);
	}	formats[9];
	struct archive_format_descriptor	*format; /* Active format. */

	/*
	 * Various information needed by archive_extract.
	 */
	struct extract		 *extract;
	int			(*cleanup_archive_extract)(struct archive_read *);
};

int	__archive_read_register_format(struct archive_read *a,
	    void *format_data,
	    const char *name,
	    int (*bid)(struct archive_read *),
	    int (*options)(struct archive_read *, const char *, const char *),
	    int (*read_header)(struct archive_read *, struct archive_entry *),
#if ARCHIVE_VERSION_NUMBER < 3000000
	    int (*read_data)(struct archive_read *, const void **, size_t *, off_t *),
#else
	    int (*read_data)(struct archive_read *, const void **, size_t *, int64_t *),
#endif
	    int (*read_data_skip)(struct archive_read *),
	    int (*cleanup)(struct archive_read *));

const void *__archive_read_ahead(struct archive_read *, size_t, ssize_t *);
int64_t	__archive_read_consume(struct archive_read *, int64_t);

#endif
