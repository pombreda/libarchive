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
__FBSDID("$FreeBSD$");

/* The sample has some files in a directory with a very long name. */
#define TESTPATH "abcdefghijklmnopqrstuvwxyz/"	\
	"abcdefghijklmnopqrstuvwxyz/"	\
	"abcdefghijklmnopqrstuvwxyz/"	\
	"abcdefghijklmnopqrstuvwxyz/"	\
	"abcdefghijklmnopqrstuvwxyz/"	\
	"abcdefghijklmnopqrstuvwxyz/"	\
	"abcdefghijklmnopqrstuvwxyz/"

/*
 * Apple shipped an extended version of GNU tar with Mac OX X 10.5
 * and earlier.
 */
DEFINE_TEST(test_compat_mac_gnutar)
{
	char name[] = "test_compat_mac_gnutar.tar.Z";
	struct archive_entry *ae;
	struct archive *a;
	const void *attr;
	size_t attrSize;
	int r;

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_compression_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	extract_reference_file(name);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, name, 10240));

	assertEqualIntA(a, ARCHIVE_OK, r = archive_read_next_header(a, &ae));
	assertEqualString(TESTPATH, archive_entry_pathname(ae));
	assertEqualInt(1275688109, archive_entry_mtime(ae));
	assertEqualInt(95594, archive_entry_uid(ae));
	assertEqualString("kientzle", archive_entry_uname(ae));
	assertEqualInt(5000, archive_entry_gid(ae));
	assertEqualString("", archive_entry_gname(ae));
	assertEqualInt(040755, archive_entry_mode(ae));

	attr = archive_entry_mac_metadata(ae, &attrSize);
	assert(attr == NULL);
	assertEqualInt(0, attrSize);

	assertEqualIntA(a, ARCHIVE_OK, r = archive_read_next_header(a, &ae));
	assertEqualString(TESTPATH "dir/", archive_entry_pathname(ae));
	assertEqualInt(1275687611, archive_entry_mtime(ae));
	assertEqualInt(95594, archive_entry_uid(ae));
	assertEqualString("kientzle", archive_entry_uname(ae));
	assertEqualInt(5000, archive_entry_gid(ae));
	assertEqualString("", archive_entry_gname(ae));
	assertEqualInt(040755, archive_entry_mode(ae));

	attr = archive_entry_mac_metadata(ae, &attrSize);
	assert(attr != NULL);
	assertEqualInt(225, attrSize);

	assertEqualIntA(a, ARCHIVE_OK, r = archive_read_next_header(a, &ae));
	assertEqualString(TESTPATH "file", archive_entry_pathname(ae));
	assertEqualInt(1275687588, archive_entry_mtime(ae));
	assertEqualInt(95594, archive_entry_uid(ae));
	assertEqualString("kientzle", archive_entry_uname(ae));
	assertEqualInt(5000, archive_entry_gid(ae));
	assertEqualString("", archive_entry_gname(ae));
	assertEqualInt(0100644, archive_entry_mode(ae));

	attr = archive_entry_mac_metadata(ae, &attrSize);
	assert(attr != NULL);
	assertEqualInt(225, attrSize);

	assertEqualIntA(a, ARCHIVE_OK, r = archive_read_next_header(a, &ae));
	assertEqualString("dir/", archive_entry_pathname(ae));
	assertEqualInt(1275688064, archive_entry_mtime(ae));
	assertEqualInt(95594, archive_entry_uid(ae));
	assertEqualString("kientzle", archive_entry_uname(ae));
	assertEqualInt(5000, archive_entry_gid(ae));
	assertEqualString("", archive_entry_gname(ae));
	assertEqualInt(040755, archive_entry_mode(ae));

	attr = archive_entry_mac_metadata(ae, &attrSize);
	assert(attr != NULL);
	assertEqualInt(225, attrSize);

	assertEqualIntA(a, ARCHIVE_OK, r = archive_read_next_header(a, &ae));
	assertEqualString("file", archive_entry_pathname(ae));
	assertEqualInt(1275625860, archive_entry_mtime(ae));
	assertEqualInt(95594, archive_entry_uid(ae));
	assertEqualString("kientzle", archive_entry_uname(ae));
	assertEqualInt(5000, archive_entry_gid(ae));
	assertEqualString("", archive_entry_gname(ae));
	assertEqualInt(0100644, archive_entry_mode(ae));

	attr = archive_entry_mac_metadata(ae, &attrSize);
	assert(attr != NULL);
	assertEqualInt(225, attrSize);

	/* Verify the end-of-archive. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	/* Verify that the format detection worked. */
	assertEqualInt(archive_compression(a), ARCHIVE_COMPRESSION_COMPRESS);
	assertEqualInt(archive_format(a), ARCHIVE_FORMAT_TAR_GNUTAR);

	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

