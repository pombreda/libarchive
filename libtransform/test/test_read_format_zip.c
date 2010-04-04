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
__FBSDID("$FreeBSD: head/lib/libtransform/test/test_read_format_zip.c 189482 2009-03-07 03:30:35Z kientzle $");

/*
 * The reference file for this has been manually tweaked so that:
 *   * file2 has length-at-end but file1 does not
 *   * file2 has an invalid CRC
 */

DEFINE_TEST(test_read_format_zip)
{
	const char *refname = "test_read_format_zip.zip";
	struct transform_entry *ae;
	struct transform *a;
	char *buff[128];
	const void *pv;
	size_t s;
	int64_t o;
	int r;

	extract_reference_file(refname);
	assert((a = transform_read_new()) != NULL);
	assertA(0 == transform_read_support_compression_all(a));
	assertA(0 == transform_read_support_format_all(a));
	assertA(0 == transform_read_open_filename(a, refname, 10240));
	assertA(0 == transform_read_next_header(a, &ae));
	assertEqualString("dir/", transform_entry_pathname(ae));
	assertEqualInt(1179604249, transform_entry_mtime(ae));
	assertEqualInt(0, transform_entry_size(ae));
	assertEqualIntA(a, TRANSFORM_EOF,
	    transform_read_data_block(a, &pv, &s, &o));
	assertEqualInt((int)s, 0);
	assertA(0 == transform_read_next_header(a, &ae));
	assertEqualString("file1", transform_entry_pathname(ae));
	assertEqualInt(1179604289, transform_entry_mtime(ae));
	assertEqualInt(18, transform_entry_size(ae));
	failure("transform_read_data() returns number of bytes read");
	r = transform_read_data(a, buff, 19);
	if (r < TRANSFORM_OK) {
		if (strcmp(transform_error_string(a),
		    "libtransform compiled without deflate support (no libz)") == 0) {
			skipping("Skipping ZIP compression check: %s",
			    transform_error_string(a));
			goto finish;
		}
	}
	assertEqualInt(18, r);
	assert(0 == memcmp(buff, "hello\nhello\nhello\n", 18));
	assertA(0 == transform_read_next_header(a, &ae));
	assertEqualString("file2", transform_entry_pathname(ae));
	assertEqualInt(1179605932, transform_entry_mtime(ae));
	failure("file2 has length-at-end, so we shouldn't see a valid size");
	assertEqualInt(0, transform_entry_size_is_set(ae));
	failure("file2 has a bad CRC, so reading to end should fail");
	assertEqualInt(TRANSFORM_WARN, transform_read_data(a, buff, 19));
	assert(0 == memcmp(buff, "hello\nhello\nhello\n", 18));
	assertA(transform_compression(a) == TRANSFORM_FILTER_NONE);
	assertA(transform_format(a) == TRANSFORM_FORMAT_ZIP);
	assertEqualIntA(a, TRANSFORM_OK, transform_read_close(a));
finish:
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));
}


