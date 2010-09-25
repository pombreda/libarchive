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

void *
setup_file(const char *filename, struct stat *st)
{
	void *src;
	int fd;
	extract_reference_file(filename);
	assertEqualInt(0, stat(filename, st));
	assert(NULL != (src = malloc(st->st_size)));
	if (src) {
		fd = open(filename, O_RDONLY);
		assert(fd >= 0);
		assertEqualInt(st->st_size, read(fd, src, st->st_size));
		close(0);
	}
	return src;
}


static void
read_test(const char *filename)
{
	struct transform *t;
	struct stat st;
	void *src, *trg;
	ssize_t avail = 0;

	src = setup_file(filename, &st);
	assert(src != NULL);
	if (!src)
		return;

	/* verify basic functionality */

	assert((t = transform_read_new()) != NULL);
	assertEqualIntA(t, TRANSFORM_OK, transform_read_add_window(t, 1, 100));
	assertEqualIntA(t, TRANSFORM_OK, transform_read_open_filename(t, filename, (10 * 1024)));

	trg = (void *)transform_read_ahead(t, 1, &avail);
	assert(NULL != trg);
	assertEqualInt(avail, 100);

	assert(0 == memcmp(trg, src + 1, 100));

	assertEqualIntA(t, 100, transform_read_consume(t, 100));

	trg = (void *)transform_read_ahead(t, 1, &avail);
	assert(trg == NULL);
	assertEqualInt(avail, 0);
	assertEqualIntA(t, TRANSFORM_FATAL, transform_read_consume(t, 1));
	transform_read_free(t);

	/* verify behaviour when the start is larger than the source */
	assert((t = transform_read_new()) != NULL);
	assertEqualIntA(t, TRANSFORM_OK, transform_read_add_window(t, st.st_size, 1000));
	/* should blow up immediately, since the window preceedes even accessing the data */
	assertEqualIntA(t, TRANSFORM_FATAL, transform_read_open_filename(t, filename, (10 * 1024)));
	transform_read_free(t);

	/* verify behaviour the boundary for EOF */
	assert((t = transform_read_new()) != NULL);
	assertEqualIntA(t, TRANSFORM_OK, transform_read_add_window(t, st.st_size -1, 1000));
	assertEqualIntA(t, TRANSFORM_OK, transform_read_open_filename(t, filename, (10 * 1024)));
	trg = (void *)transform_read_ahead(t, 1, &avail);
	assert(NULL != trg);
	assertEqualInt(avail, 1);
	assertEqualMem(trg, src + st.st_size -1, 1);
	assertEqualInt(1, transform_read_consume(t, 1));

	trg = (void *)transform_read_ahead(t, 1, &avail);
	assert(NULL == trg);
	assertEqualInt(avail, 0);
	transform_read_free(t);

	/* verify pass thru.  stupid usage, but it simplifies client usage in
	 * allowing it. */
	assert((t = transform_read_new()) != NULL);
	assertEqualIntA(t, TRANSFORM_OK, transform_read_add_window(t, 0, -1));
	assertEqualIntA(t, TRANSFORM_OK, transform_read_open_filename(t, filename, (10 * 1024)));
	trg = (void *)transform_read_ahead(t, st.st_size, &avail);
	assert(NULL != trg);
	assertEqualInt(avail, st.st_size);
	assertEqualInt(0, memcmp(trg, src, st.st_size));
	assertEqualInt(st.st_size, transform_read_consume(t, st.st_size));

	trg = (void *)transform_read_ahead(t, 1, &avail);
	assert(NULL == trg);
	assertEqualInt(avail, 0);

	/* verify when the window is larger than the upstream */
	assert((t = transform_read_new()) != NULL);
	assertEqualIntA(t, TRANSFORM_OK, transform_read_add_window(t, 0, st.st_size + 10));
	assertEqualIntA(t, TRANSFORM_OK, transform_read_open_filename(t, filename, (10 * 1024)));
	trg = (void *)transform_read_ahead(t, st.st_size, &avail);
	assert(NULL != trg);
	assertEqualInt(avail, st.st_size);
	assertEqualMem(trg, src, st.st_size);
	assertEqualInt(st.st_size, transform_read_consume(t, st.st_size));

	trg = (void *)transform_read_ahead(t, 1, &avail);
	assert(NULL == trg);
	assertEqualInt(avail, 0);

	free(src);
	transform_read_free(t);
}

static void
test_read_ahead(char *filename)
{
	struct transform *t;
	struct stat st;
	ssize_t avail = 0;
	void *src, *trg;
	
	src = setup_file(filename, &st);
	assert(src != NULL);
	if (!src)
		return;

	/* verify pass thru read_ahead semantics. */
	assert((t = transform_read_new()) != NULL);
	assertEqualIntA(t, TRANSFORM_OK, transform_read_add_window(t, 0, -1));
	assertEqualIntA(t, TRANSFORM_OK, transform_read_open_filename(t, filename, (10 * 1024)));
	trg = (void *)transform_read_ahead(t, 1, &avail);
	assert(NULL != trg);
	if (trg) {
		assert(avail < st.st_size);
	}
	trg = (void *)transform_read_ahead(t, st.st_size, &avail);
	assert(NULL != trg);
	if (trg) {
		assertEqualInt(avail, st.st_size);
		assertEqualInt(0, memcmp(trg, src, st.st_size));
		assertEqualInt(st.st_size, transform_read_consume(t, st.st_size));
	}
	free(src);
	transform_read_free(t);
}

/* Verify that attempting to open an invalid fd returns correct error. */
DEFINE_TEST(test_read_window)
{
	read_test("test_compat_bzip2_1.tbz");
	test_read_ahead("test_read_format_gtar_sparse_1_17_posix00.tar");
}
