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
 */

#include "transform_platform.h"
__FBSDID("$FreeBSD: head/lib/libarchive/archive_virtual.c 201098 2009-12-28 02:58:14Z kientzle $");

#include "transform.h"
#include "transform_private.h"

int
archive_filter_code(struct transform *a, int n)
{
	return ((a->vtable->archive_filter_code)(a, n));
}

int
archive_filter_count(struct transform *a)
{
	return ((a->vtable->archive_filter_count)(a));
}

const char *
archive_filter_name(struct transform *a, int n)
{
	return ((a->vtable->archive_filter_name)(a, n));
}

int64_t
archive_filter_bytes(struct transform *a, int n)
{
	return ((a->vtable->archive_filter_bytes)(a, n));
}

int
transform_write_close(struct transform *a)
{
	return ((a->vtable->archive_close)(a));
}

int
transform_read_close(struct transform *a)
{
	return ((a->vtable->archive_close)(a));
}

int
transform_write_free(struct transform *a)
{
	return ((a->vtable->archive_free)(a));
}

#if ARCHIVE_VERSION_NUMBER < 4000000
/* For backwards compatibility; will be removed with libarchive 4.0. */
int
transform_write_finish(struct transform *a)
{
	return ((a->vtable->archive_free)(a));
}
#endif

int
transform_read_free(struct transform *a)
{
	return ((a->vtable->archive_free)(a));
}

#if ARCHIVE_VERSION_NUMBER < 4000000
/* For backwards compatibility; will be removed with libarchive 4.0. */
int
transform_read_finish(struct transform *a)
{
	return ((a->vtable->archive_free)(a));
}
#endif

int
transform_write_finish_entry(struct transform *a)
{
	return ((a->vtable->transform_write_finish_entry)(a));
}

ssize_t
transform_write_data(struct transform *a, const void *buff, size_t s)
{
	return ((a->vtable->transform_write_data)(a, buff, s));
}

#if ARCHIVE_VERSION_NUMBER < 3000000
ssize_t
transform_write_data_block(struct transform *a, const void *buff, size_t s, off_t o)
#else
ssize_t
transform_write_data_block(struct transform *a, const void *buff, size_t s, int64_t o)
#endif
{
	return ((a->vtable->transform_write_data_block)(a, buff, s, o));
}
