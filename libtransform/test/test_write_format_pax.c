/*-
 * Copyright (c) 2003-2008 Tim Kientzle
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
__FBSDID("$FreeBSD$");

char buff2[64];

DEFINE_TEST(test_write_format_pax)
{
	size_t buffsize = 1000000;
	char *buff;
	struct transform_entry *ae;
	struct transform *a;
	size_t used;
	int i;
	char nulls[1024];
	int64_t offset, length;

	buff = malloc(buffsize); /* million bytes of work area */
	assert(buff != NULL);

	/* Create a new transform in memory. */
	assert((a = transform_write_new()) != NULL);
	assertA(0 == transform_write_set_format_pax(a));
	assertA(0 == transform_write_set_compression_none(a));
	assertA(0 == transform_write_open_memory(a, buff, buffsize, &used));

	/*
	 * "file" has a bunch of attributes and 8 bytes of data.
	 */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_set_atime(ae, 2, 20);
	transform_entry_set_birthtime(ae, 3, 30);
	transform_entry_set_ctime(ae, 4, 40);
	transform_entry_set_mtime(ae, 5, 50);
	transform_entry_copy_pathname(ae, "file");
	transform_entry_set_mode(ae, S_IFREG | 0755);
	transform_entry_set_size(ae, 8);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_header(a, ae));
	transform_entry_free(ae);
	assertEqualIntA(a, 8, transform_write_data(a, "12345678", 9));

	/*
	 * "file2" is similar but has birthtime later than mtime.
	 */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_set_atime(ae, 2, 20);
	transform_entry_set_birthtime(ae, 8, 80);
	transform_entry_set_ctime(ae, 4, 40);
	transform_entry_set_mtime(ae, 5, 50);
	transform_entry_copy_pathname(ae, "file2");
	transform_entry_set_mode(ae, S_IFREG | 0755);
	transform_entry_set_size(ae, 8);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_header(a, ae));
	transform_entry_free(ae);
	assertEqualIntA(a, 8, transform_write_data(a, "12345678", 9));

	/*
	 * "file3" is sparse file and has hole size of which is
	 * 1024000 bytes, and has 8 bytes data after the hole.
	 */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_set_atime(ae, 2, 20);
	transform_entry_set_birthtime(ae, 3, 30);
	transform_entry_set_ctime(ae, 4, 40);
	transform_entry_set_mtime(ae, 5, 50);
	transform_entry_copy_pathname(ae, "file3");
	transform_entry_set_mode(ae, S_IFREG | 0755);
	transform_entry_set_size(ae, 1024008);
	transform_entry_sparse_add_entry(ae, 1024000, 8);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_header(a, ae));
	transform_entry_free(ae);
	memset(nulls, 0, sizeof(nulls));
	for (i = 0; i < 1024000; i += 1024)
		/* write hole data, which won't be stored into an transform file. */
		assertEqualIntA(a, 1024, transform_write_data(a, nulls, 1024));
	assertEqualIntA(a, 8, transform_write_data(a, "12345678", 9));

	/*
	 * XXX TODO XXX Archive directory, other file types.
	 * Archive extended attributes, ACLs, other metadata.
	 * Verify they get read back correctly.
	 */

	/* Close out the transform. */
	assertEqualIntA(a, TRANSFORM_OK, transform_write_close(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_write_free(a));

	/*
	 *
	 * Now, read the data back.
	 *
	 */
	assert((a = transform_read_new()) != NULL);
	assertEqualIntA(a, 0, transform_read_support_format_all(a));
	assertEqualIntA(a, 0, transform_read_support_compression_all(a));
	assertEqualIntA(a, 0, transform_read_open_memory(a, buff, used));

	/*
	 * Read "file"
	 */
	assertEqualIntA(a, 0, transform_read_next_header(a, &ae));
	assertEqualInt(2, transform_entry_atime(ae));
	assertEqualInt(20, transform_entry_atime_nsec(ae));
	assertEqualInt(3, transform_entry_birthtime(ae));
	assertEqualInt(30, transform_entry_birthtime_nsec(ae));
	assertEqualInt(4, transform_entry_ctime(ae));
	assertEqualInt(40, transform_entry_ctime_nsec(ae));
	assertEqualInt(5, transform_entry_mtime(ae));
	assertEqualInt(50, transform_entry_mtime_nsec(ae));
	assertEqualString("file", transform_entry_pathname(ae));
	assert((S_IFREG | 0755) == transform_entry_mode(ae));
	assertEqualInt(8, transform_entry_size(ae));
	assertEqualIntA(a, 8, transform_read_data(a, buff2, 10));
	assertEqualMem(buff2, "12345678", 8);

	/*
	 * Read "file2"
	 */
	assertEqualIntA(a, 0, transform_read_next_header(a, &ae));
	assert(transform_entry_atime_is_set(ae));
	assertEqualInt(2, transform_entry_atime(ae));
	assertEqualInt(20, transform_entry_atime_nsec(ae));
	/* Birthtime > mtime above, so it doesn't get stored at all. */
	assert(!transform_entry_birthtime_is_set(ae));
	assertEqualInt(0, transform_entry_birthtime(ae));
	assertEqualInt(0, transform_entry_birthtime_nsec(ae));
	assert(transform_entry_ctime_is_set(ae));
	assertEqualInt(4, transform_entry_ctime(ae));
	assertEqualInt(40, transform_entry_ctime_nsec(ae));
	assert(transform_entry_mtime_is_set(ae));
	assertEqualInt(5, transform_entry_mtime(ae));
	assertEqualInt(50, transform_entry_mtime_nsec(ae));
	assertEqualString("file2", transform_entry_pathname(ae));
	assert((S_IFREG | 0755) == transform_entry_mode(ae));
	assertEqualInt(8, transform_entry_size(ae));
	assertEqualIntA(a, 8, transform_read_data(a, buff2, 10));
	assertEqualMem(buff2, "12345678", 8);

	/*
	 * Read "file3"
	 */
	assertEqualIntA(a, 0, transform_read_next_header(a, &ae));
	assertEqualInt(2, transform_entry_atime(ae));
	assertEqualInt(20, transform_entry_atime_nsec(ae));
	assertEqualInt(3, transform_entry_birthtime(ae));
	assertEqualInt(30, transform_entry_birthtime_nsec(ae));
	assertEqualInt(4, transform_entry_ctime(ae));
	assertEqualInt(40, transform_entry_ctime_nsec(ae));
	assertEqualInt(5, transform_entry_mtime(ae));
	assertEqualInt(50, transform_entry_mtime_nsec(ae));
	assertEqualString("file3", transform_entry_pathname(ae));
	assert((S_IFREG | 0755) == transform_entry_mode(ae));
	assertEqualInt(1024008, transform_entry_size(ae));
	assertEqualInt(1, transform_entry_sparse_reset(ae));
	assertEqualInt(TRANSFORM_OK,
	    transform_entry_sparse_next(ae, &offset, &length));
	assertEqualInt(1024000, offset);
	assertEqualInt(8, length);
	for (i = 0; i < 1024000; i += 1024) {
		int j;
		assertEqualIntA(a, 1024, transform_read_data(a, nulls, 1024));
		for (j = 0; j < 1024; j++)
			assertEqualInt(0, nulls[j]);
	}
	assertEqualIntA(a, 8, transform_read_data(a, buff2, 10));
	assertEqualMem(buff2, "12345678", 8);

	/*
	 * Verify the end of the transform.
	 */
	assertEqualIntA(a, TRANSFORM_EOF, transform_read_next_header(a, &ae));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_close(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_free(a));

	free(buff);
}
