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
__FBSDID("$FreeBSD: head/lib/libtransform/test/test_compat_gzip.c 191183 2009-04-17 01:06:31Z kientzle $");

/*
 * Verify our ability to read sample files compatibly with gunzip.
 *
 * In particular:
 *  * gunzip will read multiple gzip streams, concatenating the output
 *  * gunzip will stop at the end of a stream if the following data
 *    doesn't start with a gzip signature.
 */

/*
 * All of the sample files have the same contents; they're just
 * compressed in different ways.
 */
static void
verify(const char *name)
{
	const char *n[7] = { "f1", "f2", "f3", "d1/f1", "d1/f2", "d1/f3", NULL };
	struct transform_entry *ae;
	struct transform *a;
	int i,r;

	assert((a = transform_read_new()) != NULL);
	r = transform_read_support_compression_gzip(a);
	if (r == TRANSFORM_WARN) {
		skipping("gzip reading not fully supported on this platform");
		assertEqualInt(TRANSFORM_OK, transform_read_free(a));
		return;
	}
	assertEqualIntA(a, TRANSFORM_OK, r);
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_format_all(a));
	extract_reference_file(name);
	assertEqualIntA(a, TRANSFORM_OK, transform_read_open_filename(a, name, 200));

	/* Read entries, match up names with list above. */
	for (i = 0; i < 6; ++i) {
		failure("Could not read file %d (%s) from %s", i, n[i], name);
		assertEqualIntA(a, TRANSFORM_OK,
		    transform_read_next_header(a, &ae));
		if (r != TRANSFORM_OK) {
			transform_read_free(a);
			return;
		}
		assertEqualString(n[i], transform_entry_pathname(ae));
	}

	/* Verify the end-of-transform. */
	assertEqualIntA(a, TRANSFORM_EOF, transform_read_next_header(a, &ae));

	/* Verify that the format detection worked. */
	assertEqualInt(transform_compression(a), TRANSFORM_FILTER_GZIP);
	assertEqualString(transform_filter_name(a, 0), "gzip");
	assertEqualInt(transform_format(a), TRANSFORM_FORMAT_TAR_USTAR);

	assertEqualInt(TRANSFORM_OK, transform_read_close(a));
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));
}


DEFINE_TEST(test_compat_gzip)
{
	/* This sample has been 'split', each piece compressed separately,
	 * then concatenated.  Gunzip will emit the concatenated result. */
	/* Not supported in libtransform 2.6 and earlier */
	verify("test_compat_gzip_1.tgz");
	/* This sample has been compressed as a single stream, but then
	 * some unrelated garbage text has been appended to the end. */
	verify("test_compat_gzip_2.tgz");
}


