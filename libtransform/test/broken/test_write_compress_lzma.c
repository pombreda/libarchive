/*-
 * Copyright (c) 2007-2009 Tim Kientzle
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
__FBSDID("$FreeBSD: head/lib/libtransform/test/test_write_compress_lzma.c 191183 2009-04-17 01:06:31Z kientzle $");

/*
 * A basic exercise of lzma reading and writing.
 *
 */

DEFINE_TEST(test_write_compress_lzma)
{
	struct transform_entry *ae;
	struct transform* a;
	char *buff, *data;
	size_t buffsize, datasize;
	char path[16];
	size_t used1, used2;
	int i, r;

	buffsize = 2000000;
	assert(NULL != (buff = (char *)malloc(buffsize)));

	datasize = 10000;
	assert(NULL != (data = (char *)malloc(datasize)));
	memset(data, 0, datasize);

	/*
	 * Write a 100 files and read them all back.
	 */
	assert((a = transform_write_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_set_format_ustar(a));
	r = transform_write_set_compression_lzma(a);
	if (r == TRANSFORM_FATAL) {
		skipping("lzma writing not supported on this platform");
		assertEqualInt(TRANSFORM_OK, transform_write_free(a));
		return;
	}
	assertEqualIntA(a, TRANSFORM_OK,
	    transform_write_set_bytes_per_block(a, 10));
	assertEqualInt(TRANSFORM_FILTER_LZMA, transform_compression(a));
	assertEqualString("lzma", transform_filter_name(a, 0));
	assertEqualIntA(a, TRANSFORM_OK,
	    transform_write_open_memory(a, buff, buffsize, &used1));
	assertEqualInt(TRANSFORM_FILTER_LZMA, transform_compression(a));
	assertEqualString("lzma", transform_filter_name(a, 0));
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_set_filetype(ae, AE_IFREG);
	transform_entry_set_size(ae, datasize);
	for (i = 0; i < 100; i++) {
		sprintf(path, "file%03d", i);
		transform_entry_copy_pathname(ae, path);
		assertEqualIntA(a, TRANSFORM_OK, transform_write_header(a, ae));
		assertA(datasize
		    == (size_t)transform_write_data(a, data, datasize));
	}
	transform_entry_free(ae);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_close(a));
	assertEqualInt(TRANSFORM_OK, transform_write_free(a));

	assert((a = transform_read_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_format_all(a));
	r = transform_read_support_compression_lzma(a);
	if (r == TRANSFORM_WARN) {
		skipping("Can't verify lzma writing by reading back;"
		    " lzma reading not fully supported on this platform");
	} else {
		assertEqualIntA(a, TRANSFORM_OK,
		    transform_read_support_compression_all(a));
		assertEqualIntA(a, TRANSFORM_OK,
		    transform_read_open_memory(a, buff, used1));
		for (i = 0; i < 100; i++) {
			sprintf(path, "file%03d", i);
			if (!assertEqualInt(TRANSFORM_OK,
				transform_read_next_header(a, &ae)))
				break;
			assertEqualString(path, transform_entry_pathname(ae));
			assertEqualInt((int)datasize, transform_entry_size(ae));
		}
		assertEqualIntA(a, TRANSFORM_OK, transform_read_close(a));
	}
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));

	/*
	 * Repeat the cycle again, this time setting some compression
	 * options.
	 */
	assert((a = transform_write_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_set_format_ustar(a));
	assertEqualIntA(a, TRANSFORM_OK,
	    transform_write_set_bytes_per_block(a, 10));
	assertEqualIntA(a, TRANSFORM_OK, transform_write_set_compression_lzma(a));
	assertEqualIntA(a, TRANSFORM_WARN,
	    transform_write_set_compressor_options(a, "nonexistent-option=0"));
	assertEqualIntA(a, TRANSFORM_WARN,
	    transform_write_set_compressor_options(a, "compression-level=abc"));
	assertEqualIntA(a, TRANSFORM_WARN,
	    transform_write_set_compressor_options(a, "compression-level=99"));
	assertEqualIntA(a, TRANSFORM_OK,
	    transform_write_set_compressor_options(a, "compression-level=9"));
	assertEqualIntA(a, TRANSFORM_OK, transform_write_open_memory(a, buff, buffsize, &used2));
	for (i = 0; i < 100; i++) {
		sprintf(path, "file%03d", i);
		assert((ae = transform_entry_new()) != NULL);
		transform_entry_copy_pathname(ae, path);
		transform_entry_set_size(ae, datasize);
		transform_entry_set_filetype(ae, AE_IFREG);
		assertEqualIntA(a, TRANSFORM_OK, transform_write_header(a, ae));
		assertA(datasize == (size_t)transform_write_data(a, data, datasize));
		transform_entry_free(ae);
	}
	assertEqualIntA(a, TRANSFORM_OK, transform_write_close(a));
	assertEqualInt(TRANSFORM_OK, transform_write_free(a));


	assert((a = transform_read_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_format_all(a));
	r = transform_read_support_compression_lzma(a);
	if (r == TRANSFORM_WARN) {
		skipping("lzma reading not fully supported on this platform");
	} else {
		assertEqualIntA(a, TRANSFORM_OK,
		    transform_read_support_compression_all(a));
		assertEqualIntA(a, TRANSFORM_OK,
		    transform_read_open_memory(a, buff, used2));
		for (i = 0; i < 100; i++) {
			sprintf(path, "file%03d", i);
			failure("Trying to read %s", path);
			if (!assertEqualIntA(a, TRANSFORM_OK,
				transform_read_next_header(a, &ae)))
				break;
			assertEqualString(path, transform_entry_pathname(ae));
			assertEqualInt((int)datasize, transform_entry_size(ae));
		}
		assertEqualIntA(a, TRANSFORM_OK, transform_read_close(a));
	}
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));

	/*
	 * Repeat again, with much lower compression.
	 */
	assert((a = transform_write_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_set_format_ustar(a));
	assertEqualIntA(a, TRANSFORM_OK,
	    transform_write_set_bytes_per_block(a, 10));
	assertEqualIntA(a, TRANSFORM_OK, transform_write_set_compression_lzma(a));
	assertEqualIntA(a, TRANSFORM_OK,
	    transform_write_set_compressor_options(a, "compression-level=0"));
	assertEqualIntA(a, TRANSFORM_OK, transform_write_open_memory(a, buff, buffsize, &used2));
	for (i = 0; i < 100; i++) {
		sprintf(path, "file%03d", i);
		assert((ae = transform_entry_new()) != NULL);
		transform_entry_copy_pathname(ae, path);
		transform_entry_set_size(ae, datasize);
		transform_entry_set_filetype(ae, AE_IFREG);
		assertEqualIntA(a, TRANSFORM_OK, transform_write_header(a, ae));
		failure("Writing file %s", path);
		assertEqualIntA(a, datasize,
		    (size_t)transform_write_data(a, data, datasize));
		transform_entry_free(ae);
	}
	assertEqualIntA(a, TRANSFORM_OK, transform_write_close(a));
	assertEqualInt(TRANSFORM_OK, transform_write_free(a));

	/* Level 0 really does result in larger data. */
	failure("Compression-level=0 wrote %d bytes; default wrote %d bytes",
	    (int)used2, (int)used1);
	assert(used2 > used1);

	assert((a = transform_read_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_format_all(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_compression_all(a));
	r = transform_read_support_compression_lzma(a);
	if (r == TRANSFORM_WARN) {
		skipping("lzma reading not fully supported on this platform");
	} else {
		assertEqualIntA(a, TRANSFORM_OK,
		    transform_read_open_memory(a, buff, used2));
		for (i = 0; i < 100; i++) {
			sprintf(path, "file%03d", i);
			if (!assertEqualInt(TRANSFORM_OK,
				transform_read_next_header(a, &ae)))
				break;
			assertEqualString(path, transform_entry_pathname(ae));
			assertEqualInt((int)datasize, transform_entry_size(ae));
		}
		assertEqualIntA(a, TRANSFORM_OK, transform_read_close(a));
	}
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));

	/*
	 * Test various premature shutdown scenarios to make sure we
	 * don't crash or leak memory.
	 */
	assert((a = transform_write_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_set_compression_lzma(a));
	assertEqualInt(TRANSFORM_OK, transform_write_free(a));

	assert((a = transform_write_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_set_compression_lzma(a));
	assertEqualInt(TRANSFORM_OK, transform_write_close(a));
	assertEqualInt(TRANSFORM_OK, transform_write_free(a));

	assert((a = transform_write_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_set_format_ustar(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_write_set_compression_lzma(a));
	assertEqualInt(TRANSFORM_OK, transform_write_close(a));
	assertEqualInt(TRANSFORM_OK, transform_write_free(a));

	assert((a = transform_write_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_set_format_ustar(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_write_set_compression_lzma(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_write_open_memory(a, buff, buffsize, &used2));
	assertEqualInt(TRANSFORM_OK, transform_write_close(a));
	assertEqualInt(TRANSFORM_OK, transform_write_free(a));

	/*
	 * Clean up.
	 */
	free(data);
	free(buff);
}
