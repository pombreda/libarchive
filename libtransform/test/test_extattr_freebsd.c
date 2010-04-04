/*-
 * Copyright (c) 2003-2009 Tim Kientzle
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
__FBSDID("$FreeBSD: head/lib/libtransform/test/test_extattr_freebsd.c 201247 2009-12-30 05:59:21Z kientzle $");

#if defined(__FreeBSD__) && __FreeBSD__ > 4
#include <sys/extattr.h>
#endif

/*
 * Verify extended attribute restore-to-disk.  This test is FreeBSD-specific.
 */

DEFINE_TEST(test_extattr_freebsd)
{
#if !defined(__FreeBSD__)
	skipping("FreeBSD-specific extattr restore test");
#elif __FreeBSD__ < 5
	skipping("extattr restore supported only on FreeBSD 5.0 and later");
#else
	char buff[64];
	const char *xname;
	const void *xval;
	size_t xsize;
	struct stat st;
	struct transform *a;
	struct transform_entry *ae;
	int n, fd;
	int extattr_privilege_bug = 0;

	/*
	 * First, do a quick manual set/read of an extended attribute
	 * to verify that the local filesystem does support it.  If it
	 * doesn't, we'll simply skip the remaining tests.
	 */
	/* Create a test file and try to set an ACL on it. */
	fd = open("pretest", O_RDWR | O_CREAT, 0777);
	failure("Could not create test file?!");
	if (!assert(fd >= 0))
		return;

	errno = 0;
	n = extattr_set_fd(fd, EXTATTR_NAMESPACE_USER, "testattr", "1234", 4);
	if (n != 4 && errno == EOPNOTSUPP) {
		close(fd);
		skipping("extattr tests require that extattr support be enabled on the filesystem");
		return;
	}
	failure("extattr_set_fd(): errno=%d (%s)", errno, strerror(errno));
	assertEqualInt(4, n);
	close(fd);

	/*
	 * Repeat the above, but with file permissions set to 0000.
	 * This should work (extattr_set_fd() should follow fd
	 * permissions, not file permissions), but is known broken on
	 * some versions of FreeBSD.
	 */
	fd = open("pretest2", O_RDWR | O_CREAT, 00000);
	failure("Could not create test file?!");
	if (!assert(fd >= 0))
		return;

	n = extattr_set_fd(fd, EXTATTR_NAMESPACE_USER, "testattr", "1234", 4);
	if (n != 4) {
		skipping("Restoring xattr to an unwritable file seems to be broken on this platform");
		extattr_privilege_bug = 1;
	}
	close(fd);

	/* Create a write-to-disk object. */
	assert(NULL != (a = transform_write_disk_new()));
	transform_write_disk_set_options(a,
	    TRANSFORM_EXTRACT_TIME | TRANSFORM_EXTRACT_PERM | TRANSFORM_EXTRACT_XATTR);

	/* Populate an transform entry with an extended attribute. */
	ae = transform_entry_new();
	assert(ae != NULL);
	transform_entry_set_pathname(ae, "test0");
	transform_entry_set_mtime(ae, 123456, 7890);
	transform_entry_set_size(ae, 0);
	transform_entry_set_mode(ae, 0755);
	transform_entry_xattr_add_entry(ae, "user.foo", "12345", 5);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_header(a, ae));
	transform_entry_free(ae);

	/* Another entry; similar but with mode = 0. */
	ae = transform_entry_new();
	assert(ae != NULL);
	transform_entry_set_pathname(ae, "test1");
	transform_entry_set_mtime(ae, 12345678, 7890);
	transform_entry_set_size(ae, 0);
	transform_entry_set_mode(ae, 0);
	transform_entry_xattr_add_entry(ae, "user.bar", "123456", 6);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_header(a, ae));
	transform_entry_free(ae);

	/* Close the transform. */
	if (extattr_privilege_bug)
		/* If the bug is here, write_close will return warning. */
		assertEqualIntA(a, TRANSFORM_WARN, transform_write_close(a));
	else
		assertEqualIntA(a, TRANSFORM_OK, transform_write_close(a));
	assertEqualInt(TRANSFORM_OK, transform_write_free(a));

	/* Verify the data on disk. */
	assertEqualInt(0, stat("test0", &st));
	assertEqualInt(st.st_mtime, 123456);
	/* Verify extattr */
	n = extattr_get_file("test0", EXTATTR_NAMESPACE_USER,
	    "foo", buff, sizeof(buff));
	if (assertEqualInt(n, 5)) {
		buff[n] = '\0';
		assertEqualString(buff, "12345");
	}

	/* Verify the data on disk. */
	assertEqualInt(0, stat("test1", &st));
	assertEqualInt(st.st_mtime, 12345678);
	/* Verify extattr */
	n = extattr_get_file("test1", EXTATTR_NAMESPACE_USER,
	    "bar", buff, sizeof(buff));
	if (extattr_privilege_bug) {
		/* If we have the bug, the extattr won't have been written. */
		assertEqualInt(n, -1);
	} else {
		if (assertEqualInt(n, 6)) {
			buff[n] = '\0';
			assertEqualString(buff, "123456");
		}
	}

	/* Use libtransform APIs to read the file back into an entry and
	 * verify that the extattr was read correctly. */
	assert((a = transform_read_disk_new()) != NULL);
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_set_pathname(ae, "test0");
	assertEqualInt(TRANSFORM_OK,
	    transform_read_disk_entry_from_file(a, ae, -1, NULL));
	assertEqualInt(1, transform_entry_xattr_reset(ae));
	assertEqualInt(TRANSFORM_OK,
	    transform_entry_xattr_next(ae, &xname, &xval, &xsize));
	assertEqualString(xname, "user.foo");
	assertEqualInt(xsize, 5);
	assertEqualMem(xval, "12345", xsize);
	assertEqualIntA(a, TRANSFORM_OK, transform_read_close(a));
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));
	transform_entry_free(ae);
#endif
}
