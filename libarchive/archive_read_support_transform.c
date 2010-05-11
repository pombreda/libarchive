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
__FBSDID("$FreeBSD: head/lib/libarchive/archive_read_support_compression_all.c 201248 2009-12-30 06:12:03Z kientzle $");

#include "archive.h"
#include "archive_private.h"

int
archive_read_support_compression_all(struct archive *a)
{
	transform_read_support_compression_all(a->transform);
	return (ARCHIVE_OK);
}

int 
archive_read_support_compression_bzip2(struct archive *a)
{
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_read_support_compression_bzip2(a->transform)));
}
		
int
archive_read_support_compression_compress(struct archive *a)
{
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_read_support_compression_compress(a->transform)));
}

int
archive_read_support_compression_gzip(struct archive *a)
{
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_read_support_compression_gzip(a->transform)
		));
}
int
archive_read_support_compression_lzma(struct archive *a)
{
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_read_support_compression_lzma(a->transform)));
}

int
archive_read_support_compression_none(struct archive *a)
{
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_read_support_compression_none(a->transform)));
}

int
archive_read_support_compression_program(struct archive *a,
    const char *command)
{
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_read_support_compression_program(a->transform, command)));
}

int
archive_read_support_compression_program_signature(struct archive *a,
   const char *cmd, const void *signature, size_t signature_len)
{
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_read_support_compression_program_signature(a->transform,
			cmd, signature, signature_len)));
}

int
archive_read_support_compression_rpm(struct archive *a)
{
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_read_support_compression_rpm(a->transform)));
}

int
archive_read_support_compression_uu(struct archive *a)
{
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_read_support_compression_uu(a->transform)));
}

int
archive_read_support_compression_xz(struct archive *a)
{
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_read_support_compression_xz(a->transform)));
}

int
archive_read_support_compression_lzip(struct archive *a)
{
	return (__convert_transform_error_to_archive_error(a, a->transform,
		transform_read_support_compression_lzip(a->transform)));
}
