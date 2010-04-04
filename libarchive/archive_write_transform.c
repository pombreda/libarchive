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

#include "archive_platform.h"
__FBSDID("$FreeBSD: head/lib/libarchive/archive_write_transform.c 201248 2009-12-30 06:12:03Z kientzle $");

#include "archive.h"
#include "archive_private.h"

/* XXX these functions are prime candidates for macros... */

int
archive_write_add_filter_bzip2(struct archive *a)
{
	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
		ARCHIVE_STATE_NEW, "archive_write_add_filter_bzip2");
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_write_add_filter_bzip2(a->transform)));
}

int
archive_write_add_filter_compress(struct archive *a)
{
	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
		ARCHIVE_STATE_NEW, "archive_write_add_filter_compress");
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_write_add_filter_compress(a->transform)));
}

int
archive_write_add_filter_gzip(struct archive *a)
{
	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
		ARCHIVE_STATE_NEW, "archive_write_add_filter_gzip");
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_write_add_filter_gzip(a->transform)));
}

int 
archive_write_add_filter_lzma(struct archive *a)
{
	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
		ARCHIVE_STATE_NEW, "archive_write_add_filter_lzma");
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_write_add_filter_lzma(a->transform)));
}

int 
archive_write_add_filter_none(struct archive *a)
{
	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
		ARCHIVE_STATE_NEW, "archive_write_add_filter_none");
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_write_add_filter_none(a->transform)));
}

int archive_write_add_filter_program(struct archive *a,
    const char *cmd)
{
	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
		ARCHIVE_STATE_NEW, "archive_write_add_filter_program");
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_write_add_filter_program(a->transform, cmd)));
}

int
archive_write_add_filter_xz(struct archive *a)
{
	archive_check_magic(a, ARCHIVE_WRITE_MAGIC,
		ARCHIVE_STATE_NEW, "archive_write_add_filter_xz");
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_write_add_filter_xz(a->transform)));
}

#if ARCHIVE_VERSION_NUMBER < 4000000
int
archive_write_set_compression_bzip2(struct archive *a)
{
	int ret = transform_write_reset_filters(a->transform);
	ret = __convert_transform_error_to_archive_error(a, a->transform, ret);
	if (ARCHIVE_OK == ret)
		ret = archive_write_add_filter_bzip2(a);
	return (ret);
}

int
archive_write_set_compression_compress(struct archive *a)
{
	int ret = transform_write_reset_filters(a->transform);
	ret = __convert_transform_error_to_archive_error(a, a->transform, ret);
	if (ARCHIVE_OK != ret)
		return ret;
	return (archive_write_add_filter_compress(a));
}

int
archive_write_set_compression_gzip(struct archive *a)
{
	int ret = transform_write_reset_filters(a->transform);
	ret = __convert_transform_error_to_archive_error(a, a->transform, ret);
	if (ARCHIVE_OK != ret)
		return ret;
	return (archive_write_add_filter_gzip(a));
}

int
archive_write_set_compression_lzma(struct archive *a)
{
	int ret = transform_write_reset_filters(a->transform);
	ret = __convert_transform_error_to_archive_error(a, a->transform, ret);
	if (ARCHIVE_OK != ret)
		return ret;
	return (archive_write_add_filter_lzma(a));
}

int
archive_write_set_compression_none(struct archive *a)
{
	int ret = transform_write_reset_filters(a->transform);
	ret = __convert_transform_error_to_archive_error(a, a->transform, ret);
	if (ARCHIVE_OK != ret)
		return ret;
	return (archive_write_add_filter_none(a));
}

int
archive_write_set_compression_program(struct archive *a,
   const char *cmd)
{
	int ret = transform_write_reset_filters(a->transform);
	ret = __convert_transform_error_to_archive_error(a, a->transform, ret);
	if (ARCHIVE_OK != ret)
		return ret;
	return (archive_write_add_filter_program(a, cmd));
}

int
archive_write_set_compression_xz(struct archive *a)
{
	int ret = transform_write_reset_filters(a->transform);
	ret = __convert_transform_error_to_archive_error(a, a->transform, ret);
	if (ARCHIVE_OK != ret)
		return ret;
	return (archive_write_add_filter_xz(a));
}

#endif
