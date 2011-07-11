/*-
 * Copyright (c) 2003-2011 Tim Kientzle
 * Copyright (c) 2011 Michihiro NAKAJIMA
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

DEFINE_TEST(test_strip_components)
{
	assertMakeDir("d0", 0755);
	assertChdir("d0");
	assertMakeDir("d1", 0755);
	assertMakeDir("d1/d2", 0755);
	assertMakeDir("d1/d2/d3", 0755);
	assertMakeFile("d1/d2/f1", 0644, "");
	assertMakeHardlink("l1", "d1/d2/f1");
	assertMakeHardlink("d1/l2", "d1/d2/f1");
	if (canSymlink()) {
		assertMakeSymlink("s1", "d1/d2/f1");
		assertMakeSymlink("d1/s2", "d2/f1");
	}
	assertChdir("..");

	/*
	 * Test 1: Strip components when extracting archives.
	 */
	if (canSymlink())
		assertEqualInt(0, systemf("%s -wf test.pax d0/l1 d0/s1 d0/d1",
		    testprog));
	else
		assertEqualInt(0, systemf("%s -wf test.pax d0/l1 d0/d1",
		    testprog));

	assertMakeDir("target", 0755);
	assertChdir("target");
	assertEqualInt(0, systemf("%s -r --strip-components 2 "
	    "-f ../test.pax", testprog));
	assertChdir("..");

	failure("d0/ is too short and should not get restored");
	assertFileNotExists("target/d0");
	failure("d0/d1/ is too short and should not get restored");
	assertFileNotExists("target/d1");
	failure("d0/s1 is too short and should not get restored");
	assertFileNotExists("target/s1");
	failure("d0/d1/s2 is a symlink to something that won't be extracted");
	/* If platform supports symlinks, target/s2 is a broken symlink. */
	/* If platform does not support symlink, target/s2 doesn't exist. */
	assertFileNotExists("target/s2");
	if (canSymlink())
		assertIsSymlink("target/s2", "d2/f1");
	failure("d0/d1/d2 should be extracted");
	assertIsDir("target/d2", -1);

	/*
	 * Test 1b: Strip components extracting archives involving hardlinks.
	 *
	 * This next is a complicated case.  d0/l1, d0/d1/l2, and
	 * d0/d1/d2/f1 are all hardlinks to the same file; d0/l1 can't
	 * be extracted with --strip-components=2 and the other two
	 * can.  Remember that tar normally stores the first file with
	 * a body and the other as hardlink entries to the first
	 * appearance.  So the final result depends on the order in
	 * which these three names get archived.  If d0/l1 is first,
	 * none of the three can be restored.  If either of the longer
	 * names are first, then the two longer ones can both be
	 * restored.  Note that the "tar -cf" command above explicitly
	 * lists d0/l1 before d0/d1.
	 *
	 * It may be worth extending this test to exercise other
	 * archiving orders.
	 *
	 * Of course, this is all totally different for cpio and newc
	 * formats because the hardlink management is different.
	 * TODO: Rename this to test_strip_components_tar and create
	 * parallel tests for cpio and newc formats.
	 */
	failure("d0/l1 is too short and should not get restored");
	assertFileNotExists("target/l1");
	failure("d0/d1/l2 is a hardlink to file whose name was too short");
	assertFileNotExists("target/l2");
	failure("d0/d1/d2/f1 is a hardlink to file whose name was too short");
	assertFileNotExists("target/d2/f1");

	/*
	 * Test 2: Strip components when creating archives.
	 */
	if (canSymlink())
		assertEqualInt(0,
		    systemf("%s --strip-components 2 -wf test2.pax "
			    "d0/l1 d0/s1 d0/d1", testprog));
	else
		assertEqualInt(0,
		    systemf("%s --strip-components 2 -wf test2.pax "
			    "d0/l1 d0/d1", testprog));

	assertMakeDir("target2", 0755);
	assertChdir("target2");
	assertEqualInt(0, systemf("%s -r -f ../test2.pax", testprog));
	assertChdir("..");

	failure("d0/ is too short and should not have been archived");
	assertFileNotExists("target2/d0");
	failure("d0/d1/ is too short and should not have been archived");
	assertFileNotExists("target2/d1");
	failure("d0/s1 is too short and should not get restored");
	assertFileNotExists("target/s1");
	/* If platform supports symlinks, target/s2 is included. */
	if (canSymlink()) {
		failure("d0/d1/s2 is a symlink to something included in archive");
		assertIsSymlink("target2/s2", "d2/f1");
	}
	failure("d0/d1/d2 should be archived");
	assertIsDir("target2/d2", -1);

	/*
	 * Test 2b: Strip components creating archives involving hardlinks.
	 */
	failure("d0/l1 is too short and should not have been archived");
	assertFileNotExists("target/l1");
	failure("d0/d1/l2 is a hardlink to file whose name was too short");
	assertFileNotExists("target/l2");
	failure("d0/d1/d2/f1 is a hardlink to file whose name was too short");
	assertFileNotExists("target/d2/f1");

	/*
	 * Test 3: Strip components when copying directories.
	 */
	assertMakeDir("target3", 0755);
	if (canSymlink())
		assertEqualInt(0,
		    systemf("%s --strip-components 2 -rw "
			    "d0/l1 d0/s1 d0/d1 target3", testprog));
	else
		assertEqualInt(0,
		    systemf("%s --strip-components 2 -rw "
			    "d0/l1 d0/d1 target3", testprog));

	failure("d0/ is too short and should not have been copied");
	assertFileNotExists("target3/d0");
	failure("d0/d1/ is too short and should not have been copied");
	assertFileNotExists("target3/d1");
	failure("d0/s1 is too short and should not get restored");
	assertFileNotExists("target/s1");
	/* If platform supports symlinks, target/s2 is included. */
	if (canSymlink()) {
		failure("d0/d1/s2 is a symlink to something copied together");
		assertIsSymlink("target3/s2", "d2/f1");
	}
	failure("d0/d1/d2 should be archived");
	assertIsDir("target3/d2", -1);

	/*
	 * Test 3b: Strip components creating archives involving hardlinks.
	 */
	failure("d0/l1 is too short and should not have been copied");
	assertFileNotExists("target/l1");
	failure("d0/d1/l2 is a hardlink to file whose name was too short");
	assertFileNotExists("target/l2");
	failure("d0/d1/d2/f1 is a hardlink to file whose name was too short");
	assertFileNotExists("target/d2/f1");
}
