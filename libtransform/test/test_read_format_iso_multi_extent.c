/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2009 Michihiro NAKAJIMA
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
__FBSDID("$FreeBSD: head/lib/libtransform/test/test_read_format_iso_multi_extent.c 201247 2009-12-30 05:59:21Z kientzle $");

DEFINE_TEST(test_read_format_iso_multi_extent)
{
	const char *refname = "test_read_format_iso_multi_extent.iso.Z";
	struct transform_entry *ae;
	struct transform *a;
	const void *p;
	size_t size;
	int64_t offset;
	int i;

	extract_reference_file(refname);
	assert((a = transform_read_new()) != NULL);
	assertEqualInt(0, transform_read_support_compression_all(a));
	assertEqualInt(0, transform_read_support_format_all(a));
	assertEqualInt(TRANSFORM_OK,
	    transform_read_open_filename(a, refname, 10240));

	/* Retrieve each of the 2 files on the ISO image and
	 * verify that each one is what we expect. */
	for (i = 0; i < 2; ++i) {
		assertEqualInt(0, transform_read_next_header(a, &ae));

		if (strcmp(".", transform_entry_pathname(ae)) == 0) {
			/* '.' root directory. */
			assertEqualInt(AE_IFDIR, transform_entry_filetype(ae));
			assertEqualInt(2048, transform_entry_size(ae));
			assertEqualInt(86401, transform_entry_mtime(ae));
			assertEqualInt(0, transform_entry_mtime_nsec(ae));
			assertEqualInt(2, transform_entry_stat(ae)->st_nlink);
			assertEqualInt(1, transform_entry_uid(ae));
			assertEqualIntA(a, TRANSFORM_EOF,
			    transform_read_data_block(a, &p, &size, &offset));
			assertEqualInt((int)size, 0);
		} else if (strcmp("file", transform_entry_pathname(ae)) == 0) {
			/* A regular file. */
			assertEqualString("file", transform_entry_pathname(ae));
			assertEqualInt(AE_IFREG, transform_entry_filetype(ae));
			assertEqualInt(262280, transform_entry_size(ae));
			assertEqualInt(0,
			    transform_read_data_block(a, &p, &size, &offset));
			assertEqualInt(0, offset);
			assertEqualMem(p, "head--head--head", 16);
			assertEqualInt(86401, transform_entry_mtime(ae));
			assertEqualInt(86401, transform_entry_atime(ae));
			assertEqualInt(1, transform_entry_stat(ae)->st_nlink);
			assertEqualInt(1, transform_entry_uid(ae));
			assertEqualInt(2, transform_entry_gid(ae));
		} else {
			failure("Saw a file that shouldn't have been there");
			assertEqualString(transform_entry_pathname(ae), "");
		}
	}

	/* End of transform. */
	assertEqualInt(TRANSFORM_EOF, transform_read_next_header(a, &ae));

	/* Verify transform format. */
	assertEqualInt(transform_compression(a), TRANSFORM_FILTER_COMPRESS);
	assertEqualInt(transform_format(a), TRANSFORM_FORMAT_ISO9660_ROCKRIDGE);

	/* Close the transform. */
	assertEqualIntA(a, TRANSFORM_OK, transform_read_close(a));
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));
}


