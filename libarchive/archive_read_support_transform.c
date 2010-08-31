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
#include "archive_read_private.h"

static int
ensure_autodetect_bidder(struct archive_read *ar)
{
	if (NULL == ar->autodetect_bidder) {
		ar->autodetect_bidder = transform_read_add_autodetect(ar->archive.transform);
		if (NULL == ar->autodetect_bidder) {
			__archive_set_error_from_transform(&(ar->archive), ar->archive.transform);
			return (ARCHIVE_FATAL);
		}
	}
	return (ARCHIVE_OK);
}	

static int
common_support(struct archive *a, int adder(struct transform_read_bidder *))
{
	struct archive_read *ar = (struct archive_read *)a;

	if (ARCHIVE_FATAL == ensure_autodetect_bidder(ar)) {
		return (ARCHIVE_FATAL);
	}

	if (TRANSFORM_FATAL == (adder)(ar->autodetect_bidder))
		return (ARCHIVE_FATAL);

	return (ARCHIVE_OK);
}

int
archive_read_support_compression_all(struct archive *a)
{
	return common_support(a, transform_autodetect_add_all);
}

int 
archive_read_support_compression_bzip2(struct archive *a)
{
	return common_support(a, transform_autodetect_add_bzip2);
}
		
int
archive_read_support_compression_compress(struct archive *a)
{
	return common_support(a, transform_autodetect_add_compress);
}

int
archive_read_support_compression_gzip(struct archive *a)
{
	return common_support(a, transform_autodetect_add_gzip);
}
int
archive_read_support_compression_lzma(struct archive *a)
{
	return common_support(a, transform_autodetect_add_lzma);
}

int
archive_read_support_compression_lzip(struct archive *a)
{
	return common_support(a, transform_autodetect_add_lzip);
}

int
archive_read_support_compression_rpm(struct archive *a)
{
	return common_support(a, transform_autodetect_add_rpm);
}

int
archive_read_support_compression_uu(struct archive *a)
{
	return common_support(a, transform_autodetect_add_uu);
}

int
archive_read_support_compression_xz(struct archive *a)
{
	return common_support(a, transform_autodetect_add_xz);
}

int
archive_read_support_compression_none(struct archive *a)
{
	/* this is a pointless noop */
	return (ARCHIVE_OK);
}

int
archive_read_support_compression_program(struct archive *a,
    const char *command)
{
	return archive_read_support_compression_program_signature(a,
		command, NULL, 0);
}

int
archive_read_support_compression_program_signature(struct archive *a,
   const char *cmd, const void *signature, size_t signature_len)
{
	struct archive_read *ar = (struct archive_read *)a;

	if (ARCHIVE_FATAL == ensure_autodetect_bidder(ar)) {
		return (ARCHIVE_FATAL);
	}

	if (TRANSFORM_FATAL == transform_autodetect_add_program_signature(
		ar->autodetect_bidder, cmd, signature, signature_len)) {
		__archive_set_error_from_transform(a, a->transform);
		return (ARCHIVE_FATAL);
	}
	return (ARCHIVE_OK);
}

