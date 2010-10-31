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


DEFINE_TEST(test_compat_zip_symlink)
{
	char name[] = "test_compat_zip_symlink.zip";
	struct archive_entry *ae;
	struct archive *a;

	extract_reference_file(name);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_compression_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_zip(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, name, 10240));

	/* Read file entry. */
	assertEqualInt(ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("file", archive_entry_pathname(ae));
	assertEqualInt(AE_IFREG, archive_entry_filetype(ae));

	/* Read symlink entry. */
	/* TODO: This requires reading the central directory,
	 * since the necessary file-type bits are only stored there.
	 * Libarchive doesn't currently read the central directory.
	 */
	assertEqualInt(ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualString("link", archive_entry_pathname(ae));
	assertEqualInt(AE_IFLNK, archive_entry_filetype(ae));
	assertEqualString("file", archive_entry_symlink(ae));

	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualInt(archive_compression(a), ARCHIVE_COMPRESSION_NONE);
	assertEqualInt(archive_format(a), ARCHIVE_FORMAT_ZIP);
	assertEqualInt(ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
