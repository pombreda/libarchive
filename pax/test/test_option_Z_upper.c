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

DEFINE_TEST(test_option_Z_upper)
{
	struct stat st;

	assertMakeFile("old", 0644, "old");
	assertMakeFile("mid", 0644, "mid");
	assertMakeFile("new", 0644, "new");

	/* Does bsdpax support -s option ? */
	systemf("%s -w -s /old/bar/ old mid > NUL 2> check.err",
	    testprog);
	assertEqualInt(0, stat("check.err", &st));
	if (st.st_size != 0) {
		skipping("%s does not support -s option on this platform",
			testprog);
		return;
	}

	assertUtimes("old", 801100, 0, 901100, 0);
	assertUtimes("mid", 801122, 0, 901122, 0);
	assertUtimes("new", 801144, 0, 901144, 0);

	/* Test 1: Do not overwrite both 'old' and 'new' files during
	 * extract mode(-r) and 'mid' is overwritten by 'new'. */
	assertMakeDir("test1", 0755);
	assertEqualInt(0,
	    systemf("%s -wf test1/archive.pax old mid new "
		    ">test1/c.out 2>test1/c.err", testprog));
	assertChdir("test1");
	assertEmptyFile("c.out");
	assertEmptyFile("c.err");
	assertMakeFile("old", 0644, "xold");
	assertMakeFile("mid", 0644, "xmid");
	assertMakeFile("new", 0644, "xnew");
	assertUtimes("old", 801122, 0, 901122, 0);
	assertUtimes("mid", 801122, 0, 901122, 0);
	assertUtimes("new", 801122, 0, 901122, 0);
	assertEqualInt(0,
	    systemf("%s -rZf archive.pax -s /new/mid/ >x.out 2>x.err",
		    testprog));
	assertEmptyFile("x.out");
	assertEmptyFile("x.err");
	assertFileMtime("old", 901122, 0);
	assertFileContents("xold", 4, "old");
	assertFileMtime("mid", 901144, 0);
	assertFileContents("new", 3, "mid");
	assertFileMtime("new", 901122, 0);
	assertFileContents("xnew", 4, "new");
	assertChdir("..");

	/* Test 2: Do not overwrite both 'old' and 'new' files during copy
	 * mode(-rw) and 'mid' is overwritten by 'new'. */
	assertMakeDir("test2", 0755);
	assertChdir("test2");
	assertMakeFile("old", 0644, "xold");
	assertMakeFile("mid", 0644, "xmid");
	assertMakeFile("new", 0644, "xnew");
	assertUtimes("old", 801122, 0, 901122, 0);
	assertUtimes("mid", 801122, 0, 901122, 0);
	assertUtimes("new", 801122, 0, 901122, 0);
	assertChdir("..");
	assertEqualInt(0,
	    systemf("%s -rwZ -s /new/mid/ old mid new test2 "
		">test2/c.out 2>test2/c.err", testprog));
	assertChdir("test2");
	assertEmptyFile("c.out");
	assertEmptyFile("c.err");
	assertFileMtime("old", 901122, 0);
	assertFileContents("xold", 4, "old");
	assertFileMtime("mid", 901144, 0);
	assertFileContents("new", 3, "mid");
	assertFileMtime("new", 901122, 0);
	assertFileContents("xnew", 4, "new");
	assertChdir("..");

	/* Test 3: Do not overwrite both 'mid' and 'new' files during append
	 * mode(-wa) and 'old' is overwritten by 'mid'. */
	assertMakeDir("test3", 0755);
	assertEqualInt(0,
	    systemf("%s -wf test3/archive.pax old mid new "
		">test3/c.out 2>test3/c.err", testprog));
	assertChdir("test3");
	assertEmptyFile("c.out");
	assertEmptyFile("c.err");
	assertMakeFile("old", 0644, "xold");
	assertMakeFile("mid", 0644, "xmid");
	assertMakeFile("new", 0644, "xnew");
	assertUtimes("old", 801122, 0, 901120, 0);
	assertUtimes("mid", 801122, 0, 901120, 0);
	assertUtimes("new", 801122, 0, 901120, 0);
	assertEqualInt(0,
	    systemf("%s -waZf archive.pax -s /mid/old/ old mid new "
		">wa.out 2>wa.err", testprog));
	assertEmptyFile("wa.out");
	assertEmptyFile("wa.err");
	assertMakeDir("test31", 0755);
	assertChdir("test31");
	assertEqualInt(0, systemf("%s -rf ../archive.pax "
		">x.out 2>x.err", testprog));
	assertEmptyFile("x.out");
	assertEmptyFile("x.err");
	assertFileMtime("old", 901120, 0);
	assertFileContents("xmid", 4, "old");
	assertFileMtime("mid", 901122, 0);
	assertFileContents("mid", 3, "mid");
	assertFileMtime("new", 901144, 0);
	assertFileContents("new", 3, "new");
	assertChdir("../..");
}
