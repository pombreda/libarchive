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
__FBSDID("$FreeBSD: head/lib/libtransform/test/test_open_file.c 201247 2009-12-30 05:59:21Z kientzle $");

DEFINE_TEST(test_open_file)
{
	char buff[64];
	struct transform_entry *ae;
	struct transform *a;
	FILE *f;

	f = fopen("test.tar", "wb");
	assert(f != NULL);
	if (f == NULL)
		return;

	/* Write an transform through this FILE *. */
	assert((a = transform_write_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_set_format_ustar(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_write_set_compression_none(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_write_open_FILE(a, f));

	/*
	 * Write a file to it.
	 */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_set_mtime(ae, 1, 0);
	transform_entry_copy_pathname(ae, "file");
	transform_entry_set_mode(ae, S_IFREG | 0755);
	transform_entry_set_size(ae, 8);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_header(a, ae));
	transform_entry_free(ae);
	assertEqualIntA(a, 8, transform_write_data(a, "12345678", 9));

	/*
	 * Write a second file to it.
	 */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "file2");
	transform_entry_set_mode(ae, S_IFREG | 0755);
	transform_entry_set_size(ae, 819200);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_header(a, ae));
	transform_entry_free(ae);

	/* Close out the transform. */
	assertEqualIntA(a, TRANSFORM_OK, transform_write_close(a));
	assertEqualInt(TRANSFORM_OK, transform_write_free(a));
	fclose(f);

	/*
	 * Now, read the data back.
	 */
	f = fopen("test.tar", "rb");
	assert(f != NULL);
	if (f == NULL)
		return;
	assert((a = transform_read_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_format_all(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_compression_all(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_open_FILE(a, f));

	assertEqualIntA(a, TRANSFORM_OK, transform_read_next_header(a, &ae));
	assertEqualInt(1, transform_entry_mtime(ae));
	assertEqualInt(0, transform_entry_mtime_nsec(ae));
	assertEqualInt(0, transform_entry_atime(ae));
	assertEqualInt(0, transform_entry_ctime(ae));
	assertEqualString("file", transform_entry_pathname(ae));
	assert((S_IFREG | 0755) == transform_entry_mode(ae));
	assertEqualInt(8, transform_entry_size(ae));
	assertEqualIntA(a, 8, transform_read_data(a, buff, 10));
	assertEqualMem(buff, "12345678", 8);

	assertEqualIntA(a, TRANSFORM_OK, transform_read_next_header(a, &ae));
	assertEqualString("file2", transform_entry_pathname(ae));
	assert((S_IFREG | 0755) == transform_entry_mode(ae));
	assertEqualInt(819200, transform_entry_size(ae));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_data_skip(a));

	/* Verify the end of the transform. */
	assertEqualIntA(a, TRANSFORM_EOF, transform_read_next_header(a, &ae));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_close(a));
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));

	fclose(f);
}
