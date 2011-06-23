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
}
