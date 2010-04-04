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
 * $FreeBSD: head/lib/libarchive/transform_write_private.h 201155 2009-12-29 05:20:12Z kientzle $
 */

#ifndef __LIBARCHIVE_BUILD
#error This header is only to be used internally to libarchive.
#endif

#ifndef TRANSFORM_WRITE_PRIVATE_H_INCLUDED
#define	TRANSFORM_WRITE_PRIVATE_H_INCLUDED

#include "transform.h"
#include "transform_string.h"
#include "transform_private.h"

struct transform_write;

struct transform_write_filter {
	int64_t bytes_written;
	struct transform *archive; /* Associated archive. */
	struct transform_write_filter *next_filter; /* Who I write to. */
	int	(*options)(struct transform_write_filter *,
	    const char *key, const char *value);
	int	(*open)(struct transform_write_filter *);
	int	(*write)(struct transform_write_filter *, const void *, size_t);
	int	(*close)(struct transform_write_filter *);
	int	(*free)(struct transform_write_filter *);
	void	 *data;
	const char *name;
	int	  code;
	int	  bytes_per_block;
	int	  bytes_in_last_block;
};

#if ARCHIVE_VERSION < 4000000
void __transform_write_filters_free(struct transform *);
#endif

struct transform_write_filter *__transform_write_allocate_filter(struct transform *);

int __transform_write_output(struct transform_write *, const void *, size_t);
int __transform_write_filter(struct transform_write_filter *, const void *, size_t);
int __transform_write_open_filter(struct transform_write_filter *);
int __transform_write_close_filter(struct transform_write_filter *);

struct transform_write {
	struct transform	archive;

	/* Dev/ino of the archive being written. */
	dev_t		  skip_file_dev;
	int64_t		  skip_file_ino;

	/* Utility:  Pointer to a block of nulls. */
	const unsigned char	*nulls;
	size_t			 null_length;

	/* Callbacks to open/read/write/close archive stream. */
	archive_open_callback	*client_opener;
	transform_write_callback	*client_writer;
	archive_close_callback	*client_closer;
	void			*client_data;

	/*
	 * Blocking information.  Note that bytes_in_last_block is
	 * misleadingly named; I should find a better name.  These
	 * control the final output from all compressors, including
	 * compression_none.
	 */
	int		  bytes_per_block;
	int		  bytes_in_last_block;

	/*
	 * These control whether data within a gzip/bzip2 compressed
	 * stream gets padded or not.  If pad_uncompressed is set,
	 * the data will be padded to a full block before being
	 * compressed.  The pad_uncompressed_byte determines the value
	 * that will be used for padding.  Note that these have no
	 * effect on compression "none."
	 */
	int		  pad_uncompressed;

	/*
	 * First and last write filters in the pipeline.
	 */
	struct transform_write_filter *filter_first;
	struct transform_write_filter *filter_last;

	/*
	 * Pointers to format-specific functions for writing.  They're
	 * initialized by transform_write_set_format_XXX() calls.
	 */
	int	(*format_init)(struct transform_write *);
	int	(*format_options)(struct transform_write *,
		    const char *key, const char *value);
	int	(*format_finish_entry)(struct transform_write *);
	ssize_t	(*format_write_data)(struct transform_write *,
		    const void *buff, size_t);
	int	(*format_close)(struct transform_write *);
	int	(*format_free)(struct transform_write *);
};

#endif
