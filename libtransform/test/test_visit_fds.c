/*-
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
#include "test.h"
__FBSDID("$FreeBSD: src/lib/libtransform/test/test_bad_fd.c,v 1.2 2008/09/01 05:38:33 kientzle Exp $");

int
callback(struct transform *t, int position, int fd, void *data)
{
	int *fds = (int *)data;
	(void)t;        /* UNUSED */
	(void)position; /* UNUSED */
	fds[0] = fd;
	fds[1]++;
	return (TRANSFORM_OK);
}

#define TARGET_DATA "test_compat_bzip2_1.tbz"

/* Verify that attempting to open an invalid fd returns correct error. */
DEFINE_TEST(test_visit_fds)
{
	struct transform *t;
	int fd;
	int fds[] = {-1, 0};
	assert((t = transform_read_new()) != NULL);
	extract_reference_file(TARGET_DATA);
	fd = open(TARGET_DATA, O_RDONLY);
	assert(fd != -1);
	transform_read_support_compression_all(t);
	assertEqualIntA(t, TRANSFORM_OK, transform_read_open_fd(t, fd, 0));
	assertEqualIntA(t, TRANSFORM_OK, transform_visit_fds(t, callback, (void *)fds));
	assertEqualInt(fds[0], fd);
	assertEqualInt(fds[1], 1);
	transform_read_free(t);	
}
