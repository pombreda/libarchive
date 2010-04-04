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
__FBSDID("$FreeBSD: head/lib/libtransform/test/test_write_disk_hardlink.c 201247 2009-12-30 05:59:21Z kientzle $");

#if defined(_WIN32) && !defined(__CYGWIN__)
/* Execution bits, Group members bits and others bits do not work. */
#define UMASK 0177
#define E_MASK (~0177)
#else
#define UMASK 022
#define E_MASK (~0)
#endif

/*
 * Exercise hardlink recreation.
 *
 * File permissions are chosen so that the authoritive entry
 * has the correct permission and the non-authoritive versions
 * are just writeable files.
 */
DEFINE_TEST(test_write_disk_hardlink)
{
#if defined(__HAIKU__)
	skipping("transform_write_disk_hardlink; hardlinks are not supported on bfs");
#else
	static const char data[]="abcdefghijklmnopqrstuvwxyz";
	struct transform *ad;
	struct transform_entry *ae;
	int r;

	/* Force the umask to something predictable. */
	assertUmask(UMASK);

	/* Write entries to disk. */
	assert((ad = transform_write_disk_new()) != NULL);

	/*
	 * First, use a tar-like approach; a regular file, then
	 * a separate "hardlink" entry.
	 */

	/* Regular file. */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "link1a");
	transform_entry_set_mode(ae, S_IFREG | 0755);
	transform_entry_set_size(ae, sizeof(data));
	assertEqualIntA(ad, 0, transform_write_header(ad, ae));
	assertEqualInt(sizeof(data),
	    transform_write_data(ad, data, sizeof(data)));
	assertEqualIntA(ad, 0, transform_write_finish_entry(ad));
	transform_entry_free(ae);

	/* Link.  Size of zero means this doesn't carry data. */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "link1b");
	transform_entry_set_mode(ae, S_IFREG | 0642);
	transform_entry_set_size(ae, 0);
	transform_entry_copy_hardlink(ae, "link1a");
	assertEqualIntA(ad, 0, r = transform_write_header(ad, ae));
	if (r >= TRANSFORM_WARN) {
		assertEqualInt(TRANSFORM_WARN,
		    transform_write_data(ad, data, sizeof(data)));
		assertEqualIntA(ad, 0, transform_write_finish_entry(ad));
	}
	transform_entry_free(ae);

	/*
	 * Repeat tar approach test, but use unset to mark the
	 * hardlink as having no data.
	 */

	/* Regular file. */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "link2a");
	transform_entry_set_mode(ae, S_IFREG | 0755);
	transform_entry_set_size(ae, sizeof(data));
	assertEqualIntA(ad, 0, transform_write_header(ad, ae));
	assertEqualInt(sizeof(data),
	    transform_write_data(ad, data, sizeof(data)));
	assertEqualIntA(ad, 0, transform_write_finish_entry(ad));
	transform_entry_free(ae);

	/* Link.  Unset size means this doesn't carry data. */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "link2b");
	transform_entry_set_mode(ae, S_IFREG | 0642);
	transform_entry_unset_size(ae);
	transform_entry_copy_hardlink(ae, "link2a");
	assertEqualIntA(ad, 0, r = transform_write_header(ad, ae));
	if (r >= TRANSFORM_WARN) {
		assertEqualInt(TRANSFORM_WARN,
		    transform_write_data(ad, data, sizeof(data)));
		assertEqualIntA(ad, 0, transform_write_finish_entry(ad));
	}
	transform_entry_free(ae);

	/*
	 * Second, try an old-cpio-like approach; a regular file, then
	 * another identical one (which has been marked hardlink).
	 */

	/* Regular file. */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "link3a");
	transform_entry_set_mode(ae, S_IFREG | 0600);
	transform_entry_set_size(ae, sizeof(data));
	assertEqualIntA(ad, 0, transform_write_header(ad, ae));
	assertEqualInt(sizeof(data), transform_write_data(ad, data, sizeof(data)));
	assertEqualIntA(ad, 0, transform_write_finish_entry(ad));
	transform_entry_free(ae);

	/* Link. */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "link3b");
	transform_entry_set_mode(ae, S_IFREG | 0755);
	transform_entry_set_size(ae, sizeof(data));
	transform_entry_copy_hardlink(ae, "link3a");
	assertEqualIntA(ad, 0, r = transform_write_header(ad, ae));
	if (r > TRANSFORM_WARN) {
		assertEqualInt(sizeof(data),
		    transform_write_data(ad, data, sizeof(data)));
		assertEqualIntA(ad, 0, transform_write_finish_entry(ad));
	}
	transform_entry_free(ae);

	/*
	 * Finally, try a new-cpio-like approach, where the initial
	 * regular file is empty and the hardlink has the data.
	 */

	/* Regular file. */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "link4a");
	transform_entry_set_mode(ae, S_IFREG | 0600);
	transform_entry_set_size(ae, 0);
	assertEqualIntA(ad, 0, transform_write_header(ad, ae));
	assertEqualInt(TRANSFORM_WARN, transform_write_data(ad, data, 1));
	assertEqualIntA(ad, 0, transform_write_finish_entry(ad));
	transform_entry_free(ae);

	/* Link. */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "link4b");
	transform_entry_set_mode(ae, S_IFREG | 0755);
	transform_entry_set_size(ae, sizeof(data));
	transform_entry_copy_hardlink(ae, "link4a");
	assertEqualIntA(ad, 0, r = transform_write_header(ad, ae));
	if (r > TRANSFORM_FAILED) {
		assertEqualInt(sizeof(data),
		    transform_write_data(ad, data, sizeof(data)));
		assertEqualIntA(ad, 0, transform_write_finish_entry(ad));
	}
	transform_entry_free(ae);
	assertEqualInt(0, transform_write_free(ad));

	/* Test the entries on disk. */

	/* Test #1 */
	/* If the hardlink was successfully created and the transform
	 * doesn't carry data for it, we consider it to be
	 * non-authoritive for meta data as well.  This is consistent
	 * with GNU tar and BSD pax.  */
	assertIsReg("link1a", 0755 & ~UMASK);
	assertFileSize("link1a", sizeof(data));
	assertFileNLinks("link1a", 2);
	assertIsHardlink("link1a", "link1b");

	/* Test #2: Should produce identical results to test #1 */
	/* Note that marking a hardlink with size = 0 is treated the
	 * same as having an unset size.  This is partly for backwards
	 * compatibility (we used to not have unset tracking, so
	 * relied on size == 0) and partly to match the model used by
	 * common file formats that store a size of zero for
	 * hardlinks. */
	assertIsReg("link2a", 0755 & ~UMASK);
	assertFileSize("link2a", sizeof(data));
	assertFileNLinks("link2a", 2);
	assertIsHardlink("link2a", "link2b");

	/* Test #3 */
	assertIsReg("link3a", 0755 & ~UMASK);
	assertFileSize("link3a", sizeof(data));
	assertFileNLinks("link3a", 2);
	assertIsHardlink("link3a", "link3b");

	/* Test #4 */
	assertIsReg("link4a", 0755 & ~UMASK);
	assertFileNLinks("link4a", 2);
	assertFileSize("link4a", sizeof(data));
	assertIsHardlink("link4a", "link4b");
#endif
}
