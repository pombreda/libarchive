/*-
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

DEFINE_TEST(test_option_Y_upper)
{
	struct stat st;

	assertMakeFile("old", 0644, "old");

	/* Does bsdpax support -s option ? */
	systemf("%s -w -s /old/bar/ old > NUL 2> check.err", testprog);
	assertEqualInt(0, stat("check.err", &st));
	if (st.st_size != 0) {
		skipping("%s does not support -s option on this platform",
			testprog);
		return;
	}

	assertEqualInt(0, stat("old", &st));
	sleepUntilAfter(st.st_mtime);
	assertMakeFile("mid", 0644, "mid");
	assertMakeDir("test1", 0755);
	assertMakeFile("test1/old", 0644, "xold");
	assertMakeFile("test1/mid", 0644, "xmid");
	assertMakeFile("test1/new", 0644, "xnew");
	assertMakeDir("test2", 0755);
	assertMakeFile("test2/old", 0644, "xold");
	assertMakeFile("test2/mid", 0644, "xmid");
	assertMakeFile("test2/new", 0644, "xnew");
	assertMakeDir("test3", 0755);
	assertMakeFile("test3/old", 0644, "xold");
	assertMakeFile("test3/mid", 0644, "xmid");
	assertMakeFile("test3/new", 0644, "xnew");
	assertEqualInt(0, stat("test3/new", &st));
	sleepUntilAfter(st.st_mtime);
	assertMakeFile("new", 0644, "new");

	/* Test 1: During extract mode(-r), cannot use -Y option. */
	assertEqualInt(0,
	    systemf("%s -wf test1/archive.pax mid new old "
		    ">test1/c.out 2>test1/c.err", testprog));
	assertChdir("test1");
	assertEmptyFile("c.out");
	assertEmptyFile("c.err");
	assert(0 != systemf("%s -rYf archive.pax -s /old/new/ >x.out 2>x.err",
		testprog));
	assertEmptyFile("x.out");
	assertNonEmptyFile("x.err");
	assertChdir("..");

	/* Test 2: Do not overwrite both 'old' and 'new' files during
	 * copy mode(-rw). */
	assertEqualInt(0,
	    systemf("%s -rwY -s /new/mid/ old mid new test2 "
		">test2/c.out 2>test2/c.err", testprog));
	assertChdir("test2");
	assertEmptyFile("c.out");
	assertEmptyFile("c.err");
	failure("A file should not be overwritten");
	assertFileContents("xold", 4, "old");
	assertFileContents("xnew", 4, "new");
	failure("A file should be overwritten by 'new'");
	assertFileContents("new", 3, "mid");
	assertChdir("..");

	/* Test 3: During append mode(-wa), cannot use -Y option. */
	assertEqualInt(0,
	    systemf("%s -wf test3/archive.pax old mid new "
		">test3/c.out 2>test3/c.err", testprog));
	assertChdir("test3");
	assertEmptyFile("c.out");
	assertEmptyFile("c.err");
	assert(0 != systemf("%s -waYf archive.pax -s /old/new/ mid new old "
		">wa.out 2>wa.err", testprog));
	assertEmptyFile("wa.out");
	assertNonEmptyFile("wa.err");
	assertChdir("..");
}
