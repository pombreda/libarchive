/*-
 * Copyright (c) 2003-2010 Tim Kientzle
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
__FBSDID("$FreeBSD: head/lib/libarchive/test/test_read_format_zip.c 189482 2009-03-07 03:30:35Z kientzle $");

/*
 * TODO: tweak the reference file:
 *  a) remove some metadata from the regular file header
 *     so we can verify the data is coming from the central directory
 *  b) drop an entry from the central directory and verify
 *     that it doesn't get read.
 */

DEFINE_TEST(test_read_format_zip_with_seek)
{
	const char *refname = "test_read_format_zip.zip";
	struct archive_entry *ae;
	struct archive *a;
	char *buff[128];
	const void *pv;
	size_t s;
#if ARCHIVE_VERSION_NUMBER < 3000000
	off_t o;
#else
	int64_t o;
#endif
	int r;

	extract_reference_file(refname);
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_compression_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_filename(a, refname, 10240));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_next_header(a, &ae));
	assertEqualString("dir/", archive_entry_pathname(ae));
	assertEqualInt(1179604249, archive_entry_mtime(ae));
	assertEqualInt(0, archive_entry_size(ae));
	assertEqualIntA(a, ARCHIVE_EOF,
	    archive_read_data_block(a, &pv, &s, &o));
	assertEqualInt((int)s, 0);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_next_header(a, &ae));
	assertEqualString("file1", archive_entry_pathname(ae));
	assertEqualInt(1179604289, archive_entry_mtime(ae));
	assertEqualInt(18, archive_entry_size(ae));
	failure("archive_read_data() returns number of bytes read");
	r = archive_read_data(a, buff, 19);
	if (r < ARCHIVE_OK) {
		if (strcmp(archive_error_string(a),
		    "libarchive compiled without deflate support (no libz)") == 0) {
			skipping("Skipping ZIP compression check: %s",
			    archive_error_string(a));
			goto finish;
		}
	}
	assertEqualInt(18, r);
	assertEqualMem(buff, "hello\nhello\nhello\n", 18);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_next_header(a, &ae));
	assertEqualString("file2", archive_entry_pathname(ae));
	assertEqualInt(1179605932, archive_entry_mtime(ae));
	failure("file2 has length-at-end, so we shouldn't see a valid size");
	assertEqualInt(0, archive_entry_size_is_set(ae));
	failure("file2 has a bad CRC, so reading to end should fail");
	assertEqualInt(ARCHIVE_WARN, archive_read_data(a, buff, 19));
	assertEqualMem(buff, "hello\nhello\nhello\n", 18);
	assertEqualInt(ARCHIVE_COMPRESSION_NONE, archive_compression(a));
	assertEqualInt(ARCHIVE_FORMAT_ZIP, archive_format(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
finish:
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}


