/*-
 * Copyright (c) 2007 Kai Wang
 * Copyright (c) 2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
__FBSDID("$FreeBSD: head/lib/libtransform/test/test_write_format_ar.c 189308 2009-03-03 17:02:51Z kientzle $");

char buff[4096];
char buff2[64];
static char strtab[] = "abcdefghijklmn.o/\nggghhhjjjrrrttt.o/\niiijjjdddsssppp.o/\n";

DEFINE_TEST(test_write_format_ar)
{
	struct transform_entry *ae;
	struct transform* a;
	size_t used;

	/*
	 * First we try to create a SVR4/GNU format transform.
	 */
	assert((a = transform_write_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_set_format_ar_svr4(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_write_open_memory(a, buff, sizeof(buff), &used));

	/* write the filename table */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "//");
	transform_entry_set_size(ae, strlen(strtab));
	assertEqualIntA(a, TRANSFORM_OK, transform_write_header(a, ae));
	assertA(strlen(strtab) == (size_t)transform_write_data(a, strtab, strlen(strtab)));
	transform_entry_free(ae);

	/* write entries */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_set_mtime(ae, 1, 0);
	assert(1 == transform_entry_mtime(ae));
	transform_entry_set_mode(ae, S_IFREG | 0755);
	assert((S_IFREG | 0755) == transform_entry_mode(ae));
	transform_entry_copy_pathname(ae, "abcdefghijklmn.o");
	transform_entry_set_size(ae, 8);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_header(a, ae));
	assertA(8 == transform_write_data(a, "87654321", 15));
	transform_entry_free(ae);

	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "ggghhhjjjrrrttt.o");
	transform_entry_set_filetype(ae, AE_IFREG);
	transform_entry_set_size(ae, 7);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_header(a, ae));
	assertEqualIntA(a, 7, transform_write_data(a, "7777777", 7));
	transform_entry_free(ae);

	/* test full pathname */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "/usr/home/xx/iiijjjdddsssppp.o");
	transform_entry_set_mode(ae, S_IFREG | 0755);
	transform_entry_set_size(ae, 8);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_header(a, ae));
	assertEqualIntA(a, 8, transform_write_data(a, "88877766", 8));
	transform_entry_free(ae);

	/* trailing "/" should be rejected */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "/usr/home/xx/iiijjj/");
	transform_entry_set_size(ae, 8);
	assertA(0 != transform_write_header(a, ae));
	transform_entry_free(ae);

	/* Non regular file should be rejected */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "gfgh.o");
	transform_entry_set_mode(ae, S_IFDIR | 0755);
	transform_entry_set_size(ae, 6);
	assertA(0 != transform_write_header(a, ae));
	transform_entry_free(ae);

	assertEqualIntA(a, TRANSFORM_OK, transform_write_close(a));
	assertEqualInt(transform_position_compressed(a),
	    transform_position_uncompressed(a));
	assertEqualInt(used, transform_position_uncompressed(a));
	assertEqualInt(TRANSFORM_OK, transform_write_free(a));

	/*
	 * Now, read the data back.
	 */
	assert((a = transform_read_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_format_all(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_compression_all(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_open_memory(a, buff, used));

	assertEqualIntA(a, TRANSFORM_OK, transform_read_next_header(a, &ae));
	assertEqualInt(0, transform_entry_mtime(ae));
	assertEqualString("//", transform_entry_pathname(ae));
	assertEqualInt(0, transform_entry_size(ae));

	assertEqualIntA(a, TRANSFORM_OK, transform_read_next_header(a, &ae));
	assertEqualInt(1, transform_entry_mtime(ae));
	assertEqualString("abcdefghijklmn.o", transform_entry_pathname(ae));
	assertEqualInt(8, transform_entry_size(ae));
	assertEqualIntA(a, 8, transform_read_data(a, buff2, 10));
	assertEqualMem(buff2, "87654321", 8);

	assertEqualInt(TRANSFORM_OK, transform_read_next_header(a, &ae));
	assertEqualString("ggghhhjjjrrrttt.o", transform_entry_pathname(ae));
	assertEqualInt(7, transform_entry_size(ae));
	assertEqualIntA(a, 7, transform_read_data(a, buff2, 11));
	assertEqualMem(buff2, "7777777", 7);

	assertEqualIntA(a, 0, transform_read_next_header(a, &ae));
	assertEqualString("iiijjjdddsssppp.o", transform_entry_pathname(ae));
	assertEqualInt(8, transform_entry_size(ae));
	assertEqualIntA(a, 8, transform_read_data(a, buff2, 17));
	assertEqualMem(buff2, "88877766", 8);

	assertEqualIntA(a, TRANSFORM_OK, transform_read_close(a));
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));

	/*
	 * Then, we try to create a BSD format transform.
	 */
	memset(buff, 0, sizeof(buff));
	assert((a = transform_write_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_set_format_ar_bsd(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_write_open_memory(a, buff, sizeof(buff), &used));

	/* write a entry need long name extension */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "ttttyyyyuuuuiiii.o");
	transform_entry_set_filetype(ae, AE_IFREG);
	transform_entry_set_size(ae, 5);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_header(a, ae));
	assertEqualInt(5, transform_entry_size(ae));
	assertEqualIntA(a, 5, transform_write_data(a, "12345", 7));
	transform_entry_free(ae);

	/* write a entry with a short name */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "ttyy.o");
	transform_entry_set_filetype(ae, AE_IFREG);
	transform_entry_set_size(ae, 6);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_header(a, ae));
	assertEqualIntA(a, 6, transform_write_data(a, "555555", 7));
	transform_entry_free(ae);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_close(a));
	assertEqualInt(TRANSFORM_OK, transform_write_free(a));

	/* Now, Read the data back */
	assert((a = transform_read_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_format_all(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_compression_all(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_open_memory(a, buff, used));

	assertEqualIntA(a, 0, transform_read_next_header(a, &ae));
	assertEqualString("ttttyyyyuuuuiiii.o", transform_entry_pathname(ae));
	assertEqualInt(5, transform_entry_size(ae));
	assertEqualIntA(a, 5, transform_read_data(a, buff2, 10));
	assertEqualMem(buff2, "12345", 5);

	assertEqualIntA(a, 0, transform_read_next_header(a, &ae));
	assertEqualString("ttyy.o", transform_entry_pathname(ae));
	assertEqualInt(6, transform_entry_size(ae));
	assertEqualIntA(a, 6, transform_read_data(a, buff2, 10));
	assertEqualMem(buff2, "555555", 6);

	/* Test EOF */
	assertEqualIntA(a, TRANSFORM_EOF, transform_read_next_header(a, &ae));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_close(a));
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));
}
