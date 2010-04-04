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
__FBSDID("$FreeBSD: src/lib/libtransform/test/test_compat_tar_hardlink.c,v 1.3 2008/08/11 01:19:36 kientzle Exp $");

/*
 * Background:  There are two written standards for the tar file format.
 * The first is the POSIX 1988 "ustar" format, the second is the 2001
 * "pax extended" format that builds on the "ustar" format by adding
 * support for generic additional attributes.  Buried in the details
 * is one frustrating incompatibility:  The 1988 standard says that
 * tar readers MUST ignore the size field on hardlink entries; the
 * 2001 standard says that tar readers MUST obey the size field on
 * hardlink entries.  libtransform tries to navigate this particular
 * minefield by using auto-detect logic to guess whether it should
 * or should not obey the size field.
 *
 * This test tries to probe the boundaries of such handling; the test
 * transforms here were adapted from real transforms created by real
 * tar implementations that are (as of early 2008) apparently still
 * in use.
 */

static void
test_compat_tar_hardlink_1(void)
{
	char name[] = "test_compat_tar_hardlink_1.tar";
	struct transform_entry *ae;
	struct transform *a;

	assert((a = transform_read_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_compression_all(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_format_all(a));
	extract_reference_file(name);
	assertEqualIntA(a, TRANSFORM_OK, transform_read_open_filename(a, name, 10240));

	/* Read first entry, which is a regular file. */
	assertEqualIntA(a, TRANSFORM_OK, transform_read_next_header(a, &ae));
	assertEqualString("xmcd-3.3.2/docs_d/READMf",
		transform_entry_pathname(ae));
	assertEqualString(NULL, transform_entry_hardlink(ae));
	assertEqualInt(321, transform_entry_size(ae));
	assertEqualInt(1082575645, transform_entry_mtime(ae));
	assertEqualInt(1851, transform_entry_uid(ae));
	assertEqualInt(3, transform_entry_gid(ae));
	assertEqualInt(0100444, transform_entry_mode(ae));

	/* Read second entry, which is a hard link at the end of transform. */
	assertEqualIntA(a, TRANSFORM_OK, transform_read_next_header(a, &ae));
	assertEqualString("xmcd-3.3.2/README",
		transform_entry_pathname(ae));
	assertEqualString(
		"xmcd-3.3.2/docs_d/READMf",
		transform_entry_hardlink(ae));
	assertEqualInt(0, transform_entry_size(ae));
	assertEqualInt(1082575645, transform_entry_mtime(ae));
	assertEqualInt(1851, transform_entry_uid(ae));
	assertEqualInt(3, transform_entry_gid(ae));
	assertEqualInt(0100444, transform_entry_mode(ae));

	/* Verify the end-of-transform. */
	/*
	 * This failed in libtransform 2.4.12 because the tar reader
	 * tried to obey the size field for the hard link and ended
	 * up running past the end of the file.
	 */
	assertEqualIntA(a, TRANSFORM_EOF, transform_read_next_header(a, &ae));

	/* Verify that the format detection worked. */
	assertEqualInt(transform_compression(a), TRANSFORM_FILTER_NONE);
	assertEqualInt(transform_format(a), TRANSFORM_FORMAT_TAR);

	assertEqualInt(TRANSFORM_OK, transform_read_close(a));
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));
}

DEFINE_TEST(test_compat_tar_hardlink)
{
	test_compat_tar_hardlink_1();
}


