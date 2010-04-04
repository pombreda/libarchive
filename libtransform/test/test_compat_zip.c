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
__FBSDID("$FreeBSD: head/lib/libtransform/test/test_compat_zip.c 196962 2009-09-08 05:02:41Z kientzle $");

/* Copy this function for each test file and adjust it accordingly. */
static void
test_compat_zip_1(void)
{
	char name[] = "test_compat_zip_1.zip";
	struct transform_entry *ae;
	struct transform *a;
	int r;

	assert((a = transform_read_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_compression_all(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_format_zip(a));
	extract_reference_file(name);
	assertEqualIntA(a, TRANSFORM_OK, transform_read_open_filename(a, name, 10240));

	/* Read first entry. */
	assertEqualIntA(a, TRANSFORM_OK, transform_read_next_header(a, &ae));
	assertEqualString("META-INF/MANIFEST.MF", transform_entry_pathname(ae));

	/* Read second entry. */
	r = transform_read_next_header(a, &ae);
	if (r != TRANSFORM_OK) {
		if (strcmp(transform_error_string(a),
		    "libtransform compiled without deflate support (no libz)") == 0) {
			skipping("Skipping ZIP compression check: %s",
			    transform_error_string(a));
			goto finish;
		}
	}
	assertEqualIntA(a, TRANSFORM_OK, r);
	assertEqualString("tmp.class", transform_entry_pathname(ae));

	assertEqualIntA(a, TRANSFORM_EOF, transform_read_next_header(a, &ae));

	assertEqualInt(transform_compression(a), TRANSFORM_FILTER_NONE);
	assertEqualInt(transform_format(a), TRANSFORM_FORMAT_ZIP);

	assertEqualInt(TRANSFORM_OK, transform_read_close(a));
finish:
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));
}


DEFINE_TEST(test_compat_zip)
{
	test_compat_zip_1();
}


