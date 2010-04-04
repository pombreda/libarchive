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
#include "test.h"
__FBSDID("$FreeBSD: head/lib/libtransform/test/test_read_data_large.c 201247 2009-12-30 05:59:21Z kientzle $");

/*
 * Test read/write of a 10M block of data in a single operation.
 * Uses an in-memory transform with a single 10M entry.  Exercises
 * transform_read_data() to ensure it can handle large blocks like
 * this and also exercises transform_read_data_into_fd() (which
 * had a bug relating to this, fixed in Nov 2006).
 */

#if defined(_WIN32) && !defined(__CYGWIN__)
#define open _open
#define close _close
#endif

char buff1[11000000];
char buff2[10000000];
char buff3[10000000];

DEFINE_TEST(test_read_data_large)
{
	struct transform_entry *ae;
	struct transform *a;
	char tmpfilename[] = "largefile";
	int tmpfilefd;
	FILE *f;
	unsigned int i;
	size_t used;

	/* Create a new transform in memory. */
	assert((a = transform_write_new()) != NULL);
	assertA(0 == transform_write_set_format_ustar(a));
	assertA(0 == transform_write_set_compression_none(a));
	assertA(0 == transform_write_open_memory(a, buff1, sizeof(buff1), &used));

	/*
	 * Write a file (with random contents) to it.
	 */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "file");
	transform_entry_set_mode(ae, S_IFREG | 0755);
	for (i = 0; i < sizeof(buff2); i++)
		buff2[i] = (unsigned char)rand();
	transform_entry_set_size(ae, sizeof(buff2));
	assertA(0 == transform_write_header(a, ae));
	transform_entry_free(ae);
	assertA((int)sizeof(buff2) == transform_write_data(a, buff2, sizeof(buff2)));

	/* Close out the transform. */
	assertEqualIntA(a, TRANSFORM_OK, transform_write_close(a));
	assertEqualInt(TRANSFORM_OK, transform_write_free(a));

	/* Check that transform_read_data can handle 10*10^6 at a pop. */
	assert((a = transform_read_new()) != NULL);
	assertA(0 == transform_read_support_format_all(a));
	assertA(0 == transform_read_support_compression_all(a));
	assertA(0 == transform_read_open_memory(a, buff1, sizeof(buff1)));
	assertA(0 == transform_read_next_header(a, &ae));
	failure("Wrote 10MB, but didn't read the same amount");
	assertEqualIntA(a, sizeof(buff2),transform_read_data(a, buff3, sizeof(buff3)));
	failure("Read expected 10MB, but data read didn't match what was written");
	assert(0 == memcmp(buff2, buff3, sizeof(buff3)));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_close(a));
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));

	/* Check transform_read_data_into_fd */
	assert((a = transform_read_new()) != NULL);
	assertA(0 == transform_read_support_format_all(a));
	assertA(0 == transform_read_support_compression_all(a));
	assertA(0 == transform_read_open_memory(a, buff1, sizeof(buff1)));
	assertA(0 == transform_read_next_header(a, &ae));
#if defined(__BORLANDC__)
	tmpfilefd = open(tmpfilename, O_WRONLY | O_CREAT | O_BINARY);
#else
	tmpfilefd = open(tmpfilename, O_WRONLY | O_CREAT | O_BINARY, 0777);
#endif
	assert(tmpfilefd != 0);
	assertEqualIntA(a, 0, transform_read_data_into_fd(a, tmpfilefd));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_close(a));
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));
	close(tmpfilefd);

	f = fopen(tmpfilename, "rb");
	assert(f != NULL);
	assertEqualInt(sizeof(buff3), fread(buff3, 1, sizeof(buff3), f));
	fclose(f);
	assert(0 == memcmp(buff2, buff3, sizeof(buff3)));
}
