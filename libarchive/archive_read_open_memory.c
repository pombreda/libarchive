/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2010 Brian Harring
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
__FBSDID("$FreeBSD: src/lib/libarchive/archive_read_open_memory.c,v 1.6 2007/07/06 15:51:59 kientzle Exp $");

#include "archive.h"
#include "archive_private.h"

int
archive_read_open_memory(struct archive *a, void *buff, size_t size)
{
	return archive_read_open_memory2(a, buff, size, size);
}

/*
 * Don't use _open_memory2() in production code; the archive_read_open_memory()
 * version is the one you really want.  This is just here so that
 * test harnesses can exercise block operations inside the library.
 */
int
archive_read_open_memory2(struct archive *a, void *buff,
    size_t size, size_t read_size)
{
	int e;

	e = __convert_transform_error_to_archive_error(a, a->transform,
		transform_read_open_memory2(a->transform, buff, size, read_size));
	if (ARCHIVE_OK == e) {
		e = archive_read_open_preopened_transform(a);
	}

	return (e);
}
