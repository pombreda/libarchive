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

static void
read_test(char *filename)
{
	struct transform *t, *t_src;
	struct stat st;
	void *src, *trg;
	int fd;
	ssize_t avail = 0;
	extract_reference_file(filename);
	assertEqualInt(0, stat(filename, &st));
	assert(NULL != (src = malloc(st.st_size)));
	fd = open(filename, O_RDONLY);
	assert(fd >= 0);
	assertEqualInt(st.st_size, read(fd, src, st.st_size));
	close(0);


	/* assert it doesn't muck with things, and is a straight pass thru where possible */
	assert((t = transform_read_new()) != NULL);
	assert((t_src = transform_read_new()) != NULL);
	assertEqualIntA(t_src, TRANSFORM_OK, transform_read_open_filename(t_src, filename, (10 * 1024)));
	assertEqualIntA(t, TRANSFORM_OK, transform_read_open_transform(t, t_src, 0, -1));
	trg = (void *)transform_read_ahead(t, st.st_size, &avail);
	assertEqualMem(trg, src, st.st_size);
	assertEqualIntA(t, st.st_size, transform_read_consume(t, st.st_size));
	assert(NULL == transform_read_ahead(t, 1, &avail));
	transform_read_free(t);
	transform_read_free(t_src);

	/* simple test of limiting, and opening multiple reads into it. */
	assert((t = transform_read_new()) != NULL);
	assert((t_src = transform_read_new()) != NULL);
	assertEqualIntA(t_src, TRANSFORM_OK, transform_read_open_filename(t_src, filename, (10 * 1024)));
	assertEqualIntA(t, TRANSFORM_OK, transform_read_open_transform(t, t_src, 10, 100));
	assert(st.st_size > 200);
	trg = (void *)transform_read_ahead(t, 100, &avail);
	assert(NULL != trg);
	assertEqualMem(trg, src + 10, 100);
	assertEqualIntA(t, 100, transform_read_consume(t, 100));
	assert(NULL == transform_read_ahead(t, 1, &avail));
	transform_read_free(t);

	/* continue on. */
	assert((t = transform_read_new()) != NULL);
	assertEqualIntA(t, TRANSFORM_OK, transform_read_open_transform(t, t_src, 90, -1));
	int64_t count = st.st_size - 200;
	trg = (void *)transform_read_ahead(t, count, &avail);
	assert(NULL != trg);
	assertEqualInt(avail, count);
	assertEqualMem(trg, src + 200, count);
	assertEqualIntA(t, count, transform_read_consume(t, count));
	assert(NULL == transform_read_ahead(t, 1, &avail));

	transform_read_free(t);
	transform_read_free(t_src);
}

DEFINE_TEST(test_read_transform)
{
	read_test("test_compat_bzip2_1.tbz");
}
