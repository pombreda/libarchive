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
__FBSDID("$FreeBSD: head/lib/libtransform/test/test_write_format_cpio.c 185672 2008-12-06 06:02:26Z kientzle $");

/* The version stamp macro was introduced after cpio write support. */
static void
test_format(int	(*set_format)(struct transform *))
{
	char filedata[64];
	struct transform_entry *ae;
	struct transform *a;
	char *p;
	size_t used;
	size_t buffsize = 1000000;
	char *buff;
	int damaged = 0;

	buff = malloc(buffsize);

	/* Create a new transform in memory. */
	assert((a = transform_write_new()) != NULL);
	assertA(0 == (*set_format)(a));
	assertA(0 == transform_write_set_compression_none(a));
	assertA(0 == transform_write_open_memory(a, buff, buffsize, &used));

	/*
	 * Write a file to it.
	 */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_set_mtime(ae, 1, 10);
	assert(1 == transform_entry_mtime(ae));
	assert(10 == transform_entry_mtime_nsec(ae));
	p = strdup("file");
	transform_entry_copy_pathname(ae, p);
	strcpy(p, "XXXX");
	free(p);
	assertEqualString("file", transform_entry_pathname(ae));
	transform_entry_set_mode(ae, S_IFREG | 0755);
	assert((S_IFREG | 0755) == transform_entry_mode(ae));
	transform_entry_set_size(ae, 8);

	assertA(0 == transform_write_header(a, ae));
	transform_entry_free(ae);
	assertA(8 == transform_write_data(a, "12345678", 9));

	/*
	 * Write another file to it.
	 */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_set_mtime(ae, 1, 10);
	assert(1 == transform_entry_mtime(ae));
	assert(10 == transform_entry_mtime_nsec(ae));
	p = strdup("file2");
	transform_entry_copy_pathname(ae, p);
	strcpy(p, "XXXX");
	free(p);
	assertEqualString("file2", transform_entry_pathname(ae));
	transform_entry_set_mode(ae, S_IFREG | 0755);
	assert((S_IFREG | 0755) == transform_entry_mode(ae));
	transform_entry_set_size(ae, 4);

	assertA(0 == transform_write_header(a, ae));
	transform_entry_free(ae);
	assertA(4 == transform_write_data(a, "1234", 5));

	/*
	 * Write a directory to it.
	 */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_set_mtime(ae, 11, 110);
	transform_entry_copy_pathname(ae, "dir");
	transform_entry_set_mode(ae, S_IFDIR | 0755);
	transform_entry_set_size(ae, 512);

	assertA(0 == transform_write_header(a, ae));
	assertEqualInt(0, transform_entry_size(ae));
	transform_entry_free(ae);
	assertEqualIntA(a, 0, transform_write_data(a, "12345678", 9));


	/* Close out the transform. */
	assertEqualIntA(a, TRANSFORM_OK, transform_write_close(a));
	assertEqualInt(TRANSFORM_OK, transform_write_free(a));

	/*
	 * Damage the second entry to test the search-ahead recovery.
	 * TODO: Move the damage-recovery checking to a separate test;
	 * it doesn't really belong in this write test.
	 */
	{
		int i;
		for (i = 80; i < 150; i++) {
			if (memcmp(buff + i, "07070", 5) == 0) {
				damaged = 1;
				buff[i] = 'X';
				break;
			}
		}
	}
	failure("Unable to locate the second header for damage-recovery test.");
	assert(damaged == 1);

	/*
	 * Now, read the data back.
	 */
	assert((a = transform_read_new()) != NULL);
	assertA(0 == transform_read_support_format_all(a));
	assertA(0 == transform_read_support_compression_all(a));
	assertA(0 == transform_read_open_memory(a, buff, used));

	if (!assertEqualIntA(a, 0, transform_read_next_header(a, &ae))) {
		transform_read_free(a);
		return;
	}

	assertEqualInt(1, transform_entry_mtime(ae));
	/* Not the same as above: cpio doesn't store hi-res times. */
	assert(0 == transform_entry_mtime_nsec(ae));
	assert(0 == transform_entry_atime(ae));
	assert(0 == transform_entry_ctime(ae));
	assertEqualString("file", transform_entry_pathname(ae));
	assertEqualInt((S_IFREG | 0755), transform_entry_mode(ae));
	assertEqualInt(8, transform_entry_size(ae));
	assertA(8 == transform_read_data(a, filedata, 10));
	assert(0 == memcmp(filedata, "12345678", 8));

	/*
	 * The second file can't be read because we damaged its header.
	 */

	/*
	 * Read the dir entry back.
	 * TRANSFORM_WARN here because the damaged entry was skipped.
	 */
	assertEqualIntA(a, TRANSFORM_WARN, transform_read_next_header(a, &ae));
	assertEqualInt(11, transform_entry_mtime(ae));
	assert(0 == transform_entry_mtime_nsec(ae));
	assert(0 == transform_entry_atime(ae));
	assert(0 == transform_entry_ctime(ae));
	assertEqualString("dir", transform_entry_pathname(ae));
	assertEqualInt((S_IFDIR | 0755), transform_entry_mode(ae));
	assertEqualInt(0, transform_entry_size(ae));
	assertEqualIntA(a, 0, transform_read_data(a, filedata, 10));

	/* Verify the end of the transform. */
	assertEqualIntA(a, 1, transform_read_next_header(a, &ae));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_close(a));
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));

	free(buff);
}

DEFINE_TEST(test_write_format_cpio)
{
	test_format(transform_write_set_format_cpio);
	test_format(transform_write_set_format_cpio_newc);
}
