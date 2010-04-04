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
__FBSDID("$FreeBSD: head/lib/libtransform/test/test_write_disk_times.c 201247 2009-12-30 05:59:21Z kientzle $");

/*
 * Exercise time restores in transform_write_disk(), including
 * correct handling of omitted time values.
 * On FreeBSD, we also test birthtime and high-res time restores.
 */

DEFINE_TEST(test_write_disk_times)
{
	struct transform *a;
	struct transform_entry *ae;

	/* Create an transform_write_disk object. */
	assert((a = transform_write_disk_new()) != NULL);
	assertEqualInt(TRANSFORM_OK,
	    transform_write_disk_set_options(a, TRANSFORM_EXTRACT_TIME));

	/*
	 * Easy case: mtime and atime both specified.
	 */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "file1");
	transform_entry_set_mode(ae, S_IFREG | 0777);
	transform_entry_set_atime(ae, 123456, 0);
	transform_entry_set_mtime(ae, 234567, 0);
	assertEqualInt(TRANSFORM_OK, transform_write_header(a, ae));
	assertEqualInt(TRANSFORM_OK, transform_write_finish_entry(a));
	transform_entry_free(ae);
	/* Verify */
	assertFileAtime("file1", 123456, 0);
	assertFileMtime("file1", 234567, 0);

	/*
	 * mtime specified, but not atime
	 */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "file2");
	transform_entry_set_mode(ae, S_IFREG | 0777);
	transform_entry_set_mtime(ae, 234567, 0);
	assertEqualInt(TRANSFORM_OK, transform_write_header(a, ae));
	assertEqualInt(TRANSFORM_OK, transform_write_finish_entry(a));
	transform_entry_free(ae);
	assertFileMtime("file2", 234567, 0);
	assertFileAtimeRecent("file2");

	/*
	 * atime specified, but not mtime
	 */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "file3");
	transform_entry_set_mode(ae, S_IFREG | 0777);
	transform_entry_set_atime(ae, 345678, 0);
	assertEqualInt(TRANSFORM_OK, transform_write_header(a, ae));
	assertEqualInt(TRANSFORM_OK, transform_write_finish_entry(a));
	transform_entry_free(ae);
	/* Verify: Current mtime and atime as specified. */
	assertFileAtime("file3", 345678, 0);
	assertFileMtimeRecent("file3");

	/*
	 * Neither atime nor mtime specified.
	 */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "file4");
	transform_entry_set_mode(ae, S_IFREG | 0777);
	assertEqualInt(TRANSFORM_OK, transform_write_header(a, ae));
	assertEqualInt(TRANSFORM_OK, transform_write_finish_entry(a));
	transform_entry_free(ae);
	/* Verify: Current mtime and atime. */
	assertFileAtimeRecent("file4");
	assertFileMtimeRecent("file4");

#if defined(__FreeBSD__)
	/*
	 * High-res mtime and atime on FreeBSD.
	 */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "file10");
	transform_entry_set_mode(ae, S_IFREG | 0777);
	transform_entry_set_atime(ae, 1234567, 23456);
	transform_entry_set_mtime(ae, 2345678, 4567);
	assertEqualInt(TRANSFORM_OK, transform_write_header(a, ae));
	assertEqualInt(TRANSFORM_OK, transform_write_finish_entry(a));
	transform_entry_free(ae);
	/* Verify */
	assertFileMtime("file10", 2345678, 4567);
	assertFileAtime("file10", 1234567, 23456);

	/*
	 * Birthtime, mtime and atime on FreeBSD
	 */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "file11");
	transform_entry_set_mode(ae, S_IFREG | 0777);
	transform_entry_set_atime(ae, 1234567, 23456);
	transform_entry_set_birthtime(ae, 3456789, 12345);
	/* mtime must be later than birthtime! */
	transform_entry_set_mtime(ae, 12345678, 4567);
	assertEqualInt(TRANSFORM_OK, transform_write_header(a, ae));
	assertEqualInt(TRANSFORM_OK, transform_write_finish_entry(a));
	transform_entry_free(ae);
	/* Verify */
	assertFileAtime("file11", 1234567, 23456);
	assertFileBirthtime("file11", 3456789, 12345);
	assertFileMtime("file11", 12345678, 4567);

	/*
	 * Birthtime only on FreeBSD.
	 */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "file12");
	transform_entry_set_mode(ae, S_IFREG | 0777);
	transform_entry_set_birthtime(ae, 3456789, 12345);
	assertEqualInt(TRANSFORM_OK, transform_write_header(a, ae));
	assertEqualInt(TRANSFORM_OK, transform_write_finish_entry(a));
	transform_entry_free(ae);
	/* Verify */
	assertFileAtimeRecent("file12");
	assertFileBirthtime("file12", 3456789, 12345);
	assertFileMtimeRecent("file12");

	/*
	 * mtime only on FreeBSD.
	 */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_copy_pathname(ae, "file13");
	transform_entry_set_mode(ae, S_IFREG | 0777);
	transform_entry_set_mtime(ae, 4567890, 23456);
	assertEqualInt(TRANSFORM_OK, transform_write_header(a, ae));
	assertEqualInt(TRANSFORM_OK, transform_write_finish_entry(a));
	transform_entry_free(ae);
	/* Verify */
	assertFileAtimeRecent("file13");
	assertFileBirthtime("file13", 4567890, 23456);
	assertFileMtime("file13", 4567890, 23456);
#else
	skipping("Platform-specific time restore tests");
#endif

	assertEqualInt(TRANSFORM_OK, transform_write_free(a));
}
