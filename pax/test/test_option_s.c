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
__FBSDID("$FreeBSD$");

DEFINE_TEST(test_option_s)
{
	struct stat st;

	/* Create a sample file hierarchy. */
	assertMakeDir("in", 0755);
	assertMakeDir("in/d1", 0755);
	assertMakeFile("in/d1/foo", 0644, "foo");
	assertMakeFile("in/d1/bar", 0644, "bar");
	if (canSymlink()) {
		assertMakeFile("in/d1/realfile", 0644, "realfile");
		assertMakeSymlink("in/d1/symlink", "realfile");
	}
	assertMakeFile("in/d1/hardlink1", 0644, "hardlinkedfile");
	assertMakeHardlink("in/d1/hardlink2", "in/d1/hardlink1");

	/* Does bsdpax support -s option ? */
	systemf("%s -w -s /foo/bar/ in/d1/foo > NUL 2> check.err",
	    testprog);
	assertEqualInt(0, stat("check.err", &st));
	if (st.st_size != 0) {
		skipping("%s does not support -s option on this platform",
			testprog);
		return;
	}

	/*
	 * Test 1: Filename substitution when creating archives.
	 */
	assertMakeDir("test1", 0755);
	systemf("%s -wf test11.pax -s /foo/bar/ in/d1/foo", testprog);
	assertChdir("test1");
	systemf("%s -rf ../test11.pax", testprog);
	assertChdir("..");
	assertFileContents("foo", 3, "test1/in/d1/bar");
	systemf("%s -wf test12.pax -s /d1/d2/ in/d1/foo", testprog);
	assertChdir("test1");
	systemf("%s -rf ../test12.pax", testprog);
	assertChdir("..");
	assertFileContents("foo", 3, "test1/in/d2/foo");

	/*
	 * Test 2: Basic substitution when extracting archive.
	 */
	assertMakeDir("test2", 0755);
	systemf("%s -wf test2.pax in/d1/foo", testprog);
	assertChdir("test2");
	systemf("%s -rf ../test2.pax -s /foo/bar/", testprog);
	assertChdir("..");
	assertFileContents("foo", 3, "test2/in/d1/bar");

	/*
	 * Test 3: Files with empty names shouldn't be archived.
	 */
	systemf("%s -w -s ,in/d1/foo,, in/d1/foo | %s -v > in.lst 2>NUL",
	    testprog, testprog);
	assertEmptyFile("in.lst");

	/*
	 * Test 4: Multiple substitutions when extracting archive.
	 */
	assertMakeDir("test4", 0755);
	systemf("%s -wf test4.pax in/d1/foo in/d1/bar", testprog);
	assertChdir("test4");
	systemf("%s -rf ../test4.pax -s /foo/bar/ -s }bar}baz}", testprog);
	assertChdir("..");
	assertFileContents("foo", 3, "test4/in/d1/bar");
	assertFileContents("bar", 3, "test4/in/d1/baz");

	/*
	 * Test 5: Name-switching substitutions when extracting archive.
	 */
	assertMakeDir("test5", 0755);
	systemf("%s -wf test5.pax in/d1/foo in/d1/bar", testprog);
	assertChdir("test5");
	systemf("%s -rf ../test5.pax -s /foo/bar/ -s }bar}foo}", testprog);
	assertChdir("..");
	assertFileContents("foo", 3, "test5/in/d1/bar");
	assertFileContents("bar", 3, "test5/in/d1/foo");

	/*
	 * Test 6: symlinks get renamed by default
	 */
	if (canSymlink()) {
		/* At extraction time. */
		assertMakeDir("test6a", 0755);
		systemf("%s -wf test6a.pax in/d1", testprog);
		assertChdir("test6a");
		systemf("%s -rf ../test6a.pax -s /d1/d2/", testprog);
		assertChdir("..");
		assertFileContents("realfile", 8, "test6a/in/d2/realfile");
		assertFileContents("realfile", 8, "test6a/in/d2/symlink");
		assertIsSymlink("test6a/in/d2/symlink", "realfile");
		/* At creation time. */
		assertMakeDir("test6b", 0755);
		systemf("%s -wf test6b.pax -s /d1/d2/ in/d1", testprog);
		assertChdir("test6b");
		systemf("%s -rf ../test6b.pax", testprog);
		assertChdir("..");
		assertFileContents("realfile", 8, "test6b/in/d2/realfile");
		assertFileContents("realfile", 8, "test6b/in/d2/symlink");
		assertIsSymlink("test6b/in/d2/symlink", "realfile");
	}

	/*
	 * Test 7: selective renaming of symlink target
	 */
	if (canSymlink()) {
		/* At extraction. */
		assertMakeDir("test7a", 0755);
		systemf("%s -wf test7a.pax in/d1", testprog);
		assertChdir("test7a");
		systemf("%s -rf ../test7a.pax -s /realfile/realfile-renamed/",
		    testprog);
		assertChdir("..");
		assertFileContents(
		    "realfile", 8, "test7a/in/d1/realfile-renamed");
		assertFileContents("realfile", 8, "test7a/in/d1/symlink");
		assertIsSymlink("test7a/in/d1/symlink", "realfile-renamed");
		/* At creation. */
		assertMakeDir("test7b", 0755);
		systemf(
		    "%s -wf test7b.pax -s /realfile/realfile-renamed/ in/d1",
		    testprog);
		assertChdir("test7b");
		systemf("%s -rf ../test7b.pax", testprog);
		assertChdir("..");
		assertFileContents("realfile", 8,
		    "test7b/in/d1/realfile-renamed");
		assertFileContents("realfile", 8, "test7b/in/d1/symlink");
		assertIsSymlink("test7b/in/d1/symlink", "realfile-renamed");
	}

	/*
	 * Test 8: hardlinks get renamed by default
	 */
	/* At extraction time. */
	assertMakeDir("test8a", 0755);
	systemf("%s -wf test8a.pax in/d1", testprog);
	assertChdir("test8a");
	systemf("%s -rf ../test8a.pax -s /d1/d2/", testprog);
	assertChdir("..");
	assertIsHardlink("test8a/in/d2/hardlink1", "test8a/in/d2/hardlink2");
	/* At creation time. */
	assertMakeDir("test8b", 0755);
	systemf("%s -wf test8b.pax -s /d1/d2/ in/d1", testprog);
	assertChdir("test8b");
	systemf("%s -rf ../test8b.pax", testprog);
	assertChdir("..");
	assertIsHardlink("test8b/in/d2/hardlink1", "test8b/in/d2/hardlink2");

	/*
	 * Test 9: selective renaming of hardlink target
	 */
	/* At extraction. (assuming hardlink2 is the hardlink entry) */
	assertMakeDir("test9a", 0755);
	systemf("%s -wf test9a.pax in/d1", testprog);
	assertChdir("test9a");
	systemf("%s -rf ../test9a.pax -s /hardlink1/hardlink1-renamed/", testprog);
	assertChdir("..");
	assertIsHardlink("test9a/in/d1/hardlink1-renamed", "test9a/in/d1/hardlink2");
	/* At extraction. (assuming hardlink1 is the hardlink entry) */
	assertMakeDir("test9b", 0755);
	systemf("%s -wf test9b.pax in/d1", testprog);
	assertChdir("test9b");
	systemf("%s -rf ../test9b.pax -s /hardlink2/hardlink2-renamed/", testprog);
	assertChdir("..");
	assertIsHardlink("test9b/in/d1/hardlink1", "test9b/in/d1/hardlink2-renamed");
	/* At creation. (assuming hardlink2 is the hardlink entry) */
	assertMakeDir("test9c", 0755);
	systemf("%s -wf test9c.pax -s /hardlink1/hardlink1-renamed/ in/d1",
	    testprog);
	assertChdir("test9c");
	systemf("%s -rf ../test9c.pax", testprog);
	assertChdir("..");
	assertIsHardlink("test9c/in/d1/hardlink1-renamed", "test9c/in/d1/hardlink2");
	/* At creation. (assuming hardlink1 is the hardlink entry) */
	assertMakeDir("test9d", 0755);
	systemf("%s -wf test9d.pax -s /hardlink2/hardlink2-renamed/ in/d1",
	    testprog);
	assertChdir("test9d");
	systemf("%s -rf ../test9d.pax", testprog);
	assertChdir("..");
	assertIsHardlink("test9d/in/d1/hardlink1", "test9d/in/d1/hardlink2-renamed");

        /*
         * Test 10: renaming symlink target without repointing symlink
         */
	if (canSymlink()) {
		/* At extraction. */
		assertMakeDir("test10a", 0755);
		systemf("%s -wf test10a.pax in/d1", testprog);
		assertChdir("test10a");
		systemf("%s -rf ../test10a.pax -s /realfile/foo/S -s /foo/realfile/",
		    testprog);
		assertChdir("..");
		assertFileContents("realfile", 8, "test10a/in/d1/foo");
		assertFileContents("foo", 3, "test10a/in/d1/realfile");
		assertFileContents("foo", 3, "test10a/in/d1/symlink");
		assertIsSymlink("test10a/in/d1/symlink", "realfile");
		/* At extraction. */
		assertMakeDir("test10b", 0755);
		systemf(
		    "%s -wf test10b.pax -s /realfile/foo/S -s /foo/realfile/ in/d1",
		    testprog);
		assertChdir("test10b");
		systemf("%s -rf ../test10b.pax", testprog);
		assertChdir("..");
		assertFileContents("realfile", 8, "test10b/in/d1/foo");
		assertFileContents("foo", 3, "test10b/in/d1/realfile");
		assertFileContents("foo", 3, "test10b/in/d1/symlink");
		assertIsSymlink("test10b/in/d1/symlink", "realfile");
	}

	/*
	 * Test 11: repointing symlink without renaming file
	 */
	if (canSymlink()) {
		/* At extraction. */
		assertMakeDir("test11a", 0755);
		systemf("%s -wf test11a.pax in/d1", testprog);
		assertChdir("test11a");
		systemf("%s -rf ../test11a.pax -s /realfile/foo/sR", testprog);
		assertChdir("..");
		assertFileContents("foo", 3, "test11a/in/d1/foo");
		assertFileContents("realfile", 8, "test11a/in/d1/realfile");
		assertFileContents("foo", 3, "test11a/in/d1/symlink");
		assertIsSymlink("test11a/in/d1/symlink", "foo");
		/* At creation. */
		assertMakeDir("test11b", 0755);
		systemf("%s -wf test11b.pax -s /realfile/foo/R in/d1", testprog);
		assertChdir("test11b");
		systemf("%s -rf ../test11b.pax", testprog);
		assertChdir("..");
		assertFileContents("foo", 3, "test11b/in/d1/foo");
		assertFileContents("realfile", 8, "test11b/in/d1/realfile");
		assertFileContents("foo", 3, "test11b/in/d1/symlink");
		assertIsSymlink("test11b/in/d1/symlink", "foo");
	}

	/*
	 * Test 12: renaming hardlink target without changing hardlink.
	 * (Requires a pre-built archive, since we otherwise can't know
	 * which element will be stored as the hardlink.)
	 */
	extract_reference_file("test_option_s.tar.Z");
	assertMakeDir("test12a", 0755);
	assertChdir("test12a");
	systemf(
	    "%s -rf ../test_option_s.tar.Z -s /hardlink1/foo/H -s /foo/hardlink1/",
	    testprog);
	assertChdir("..");
	assertFileContents("foo", 3, "test12a/in/d1/hardlink1");
	assertFileContents("hardlinkedfile", 14, "test12a/in/d1/foo");
	assertFileContents("foo", 3, "test12a/in/d1/hardlink2");
	assertIsHardlink("test12a/in/d1/hardlink1", "test12a/in/d1/hardlink2");
	/* TODO: Expand this test to verify creation as well.
	 * Since either hardlink1 or hardlink2 might get stored as a hardlink,
	 * this will either requiring testing both cases and accepting either
	 * pass, or some very creative renames that can be tested regardless.
	 */

	/*
	 * Test 13: repoint hardlink without changing files
	 * (Requires a pre-built archive, since we otherwise can't know
	 * which element will be stored as the hardlink.)
	 */
	extract_reference_file("test_option_s.tar.Z");
	assertMakeDir("test13a", 0755);
	assertChdir("test13a");
	systemf(
	    "%s -rf ../test_option_s.tar.Z -s /hardlink1/foo/Rh -s /foo/hardlink1/Rh",
	    testprog);
	assertChdir("..");
	assertFileContents("foo", 3, "test13a/in/d1/foo");
	assertFileContents("hardlinkedfile", 14, "test13a/in/d1/hardlink1");
	assertFileContents("foo", 3, "test13a/in/d1/hardlink2");
	assertIsHardlink("test13a/in/d1/foo", "test13a/in/d1/hardlink2");
	/* TODO: See above; expand this test to verify renames at creation. */
}
