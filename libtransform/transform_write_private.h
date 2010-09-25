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
 * $FreeBSD: head/lib/libtransform/transform_write_private.h 201155 2009-12-29 05:20:12Z kientzle $
 */

#ifndef __LIBTRANSFORM_BUILD
#error This header is only to be used internally to libtransform.
#endif

#ifndef TRANSFORM_WRITE_PRIVATE_H_INCLUDED
#define	TRANSFORM_WRITE_PRIVATE_H_INCLUDED

#include "transform.h"
#include "transform_string.h"
#include "transform_private.h"

struct transform_write;
struct transform_write_filter;

int transform_write_open2(struct transform *, void *,
	transform_open_callback *, transform_write_callback *,
	transform_close_callback *, transform_visit_fds_callback *);

struct transform_write_filter {
	struct marker marker;
	int64_t bytes_written;
	struct transform *transform; /* Associated transform. */
	struct transform_write_filter *next_filter; /* Who I write to. */
	int	(*options)(struct transform_write_filter *,
	    const char *key, const char *value);
	int	(*open)(struct transform_write_filter *);
	int	(*write)(struct transform_write_filter *, const void *,
		const void *, size_t);
	int	(*close)(struct transform_write_filter *);
	int	(*free)(struct transform_write_filter *);
	transform_visit_fds_callback *visit_fds;
	void	 *data;
	const char *name;
	int	  code;
	int	  bytes_per_block;
	int	  bytes_in_last_block;
};

struct transform_write_filter *__transform_write_allocate_filter(struct transform *);

int __transform_write_filter(struct transform_write_filter *, const void *, size_t);
int __transform_write_open_filter(struct transform_write_filter *);
int __transform_write_close_filter(struct transform_write_filter *);

struct transform_write {
	struct transform	transform;

	/* Callbacks to open/read/write/close transform stream. */
	transform_open_callback	*client_opener;
	transform_write_callback	*client_writer;
	transform_close_callback	*client_closer;
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
	 * First and last write filters in the pipeline.
	 */
	struct transform_write_filter *filter_first;
	struct transform_write_filter *filter_last;

};

#endif
