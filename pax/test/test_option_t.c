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

DEFINE_TEST(test_option_t)
{
	assertMakeDir("d1", 0755);
	assertMakeFile("d1/file1", 0644, "d1/file1");
	assertMakeFile("d1/empty", 0644, NULL);
	assertMakeDir("d1/subd", 0755);
	assertMakeFile("d1/subd/file2", 0644, "d1/subd/file2");

	/* Test 1: -w without -t */
	assertUtimes("d1/subd/file2", 801100, 0, 901100, 0);
	assertUtimes("d1/subd", 801111, 0, 901111, 0);
	assertUtimes("d1/empty", 801122, 0, 901122, 0);
	assertUtimes("d1/file1", 801133, 0, 901133, 0);
	assertUtimes("d1", 801144, 0, 901144, 0);
	assertMakeDir("test1", 0755);
	assertEqualInt(0,
	    systemf("%s -wf test1/archive.tar d1 >test1/c.out 2>test1/c.err",
	    testprog));
	assertChdir("test1");
	assertEmptyFile("c.out");
	assertEmptyFile("c.err");
	assertEqualInt(0,
	    systemf("%s -rf archive.tar >x.out 2>x.err", testprog));
	assertEmptyFile("x.out");
	assertEmptyFile("x.err");
	assertFileContents("d1/file1", 8, "d1/file1");
	assertEmptyFile("d1/empty");
	assertIsDir("d1/subd", 0755);
	assertFileContents("d1/subd/file2", 13, "d1/subd/file2");
	assertChdir("..");
	assertFileAtimeRecent("d1");
	assertFileMtime("d1", 901144, 0);
	assertFileAtimeRecent("d1/file1");
	assertFileMtime("d1/file1", 901133, 0);
	assertFileAtime("d1/empty", 801122, 0);
	assertFileMtime("d1/empty", 901122, 0);
	assertFileAtimeRecent("d1/subd");
	assertFileMtime("d1/subd", 901111, 0);
	assertFileAtimeRecent("d1/subd/file2");
	assertFileMtime("d1/subd/file2", 901100, 0);

	/* Test 2: -w with -t */
	assertUtimes("d1/subd/file2", 801100, 0, 901100, 0);
	assertUtimes("d1/subd", 801111, 0, 901111, 0);
	assertUtimes("d1/empty", 801122, 0, 901122, 0);
	assertUtimes("d1/file1", 801133, 0, 901133, 0);
	assertUtimes("d1", 801144, 0, 901144, 0);
	assertMakeDir("test2", 0755);
	assertEqualInt(0,
	    systemf("%s -wtf test2/archive.tar d1 >test2/c.out 2>test2/c.err",
	    testprog));
	assertChdir("test2");
	assertEmptyFile("c.out");
	assertEmptyFile("c.err");
	assertEqualInt(0,
	    systemf("%s -rf archive.tar >x.out 2>x.err", testprog));
	assertEmptyFile("x.out");
	assertEmptyFile("x.err");
	assertFileContents("d1/file1", 8, "d1/file1");
	assertEmptyFile("d1/empty");
	assertIsDir("d1/subd", 0755);
	assertFileContents("d1/subd/file2", 13, "d1/subd/file2");
	assertChdir("..");
	assertFileAtime("d1", 801144, 0);
	assertFileMtime("d1", 901144, 0);
	assertFileAtime("d1/file1", 801133, 0);
	assertFileMtime("d1/file1", 901133, 0);
	assertFileAtime("d1/empty", 801122, 0);
	assertFileMtime("d1/empty", 901122, 0);
	assertFileAtime("d1/subd", 801111, 0);
	assertFileMtime("d1/subd", 901111, 0);
	assertFileAtime("d1/subd/file2", 801100, 0);
	assertFileMtime("d1/subd/file2", 901100, 0);

	/* Test 3: -rw without -t */
	assertUtimes("d1/subd/file2", 801100, 0, 901100, 0);
	assertUtimes("d1/subd", 801111, 0, 901111, 0);
	assertUtimes("d1/empty", 801122, 0, 901122, 0);
	assertUtimes("d1/file1", 801133, 0, 901133, 0);
	assertUtimes("d1", 801144, 0, 901144, 0);
	assertMakeDir("test3", 0755);
	assertEqualInt(0,
	    systemf("%s -rw d1 test3 >test3/c.out 2>test3/c.err", testprog));
	assertChdir("test3");
	assertEmptyFile("c.out");
	assertEmptyFile("c.err");
	assertFileContents("d1/file1", 8, "d1/file1");
	assertEmptyFile("d1/empty");
	assertIsDir("d1/subd", 0755);
	assertFileContents("d1/subd/file2", 13, "d1/subd/file2");
	assertChdir("..");
	assertFileAtimeRecent("d1");
	assertFileMtime("d1", 901144, 0);
	assertFileAtimeRecent("d1/file1");
	assertFileMtime("d1/file1", 901133, 0);
	assertFileAtime("d1/empty", 801122, 0);
	assertFileMtime("d1/empty", 901122, 0);
	assertFileAtimeRecent("d1/subd");
	assertFileMtime("d1/subd", 901111, 0);
	assertFileAtimeRecent("d1/subd/file2");
	assertFileMtime("d1/subd/file2", 901100, 0);

	/* Test 4: -rw with -t */
	assertUtimes("d1/subd/file2", 801100, 0, 901100, 0);
	assertUtimes("d1/subd", 801111, 0, 901111, 0);
	assertUtimes("d1/empty", 801122, 0, 901122, 0);
	assertUtimes("d1/file1", 801133, 0, 901133, 0);
	assertUtimes("d1", 801144, 0, 901144, 0);
	assertMakeDir("test4", 0755);
	assertEqualInt(0,
	    systemf("%s -rwt d1 test4 >test4/c.out 2>test4/c.err",
	    testprog));
	assertChdir("test4");
	assertEmptyFile("c.out");
	assertEmptyFile("c.err");
	assertFileContents("d1/file1", 8, "d1/file1");
	assertEmptyFile("d1/empty");
	assertIsDir("d1/subd", 0755);
	assertFileContents("d1/subd/file2", 13, "d1/subd/file2");
	assertChdir("..");
	assertFileAtime("d1", 801144, 0);
	assertFileMtime("d1", 901144, 0);
	assertFileAtime("d1/file1", 801133, 0);
	assertFileMtime("d1/file1", 901133, 0);
	assertFileAtime("d1/empty", 801122, 0);
	assertFileMtime("d1/empty", 901122, 0);
	assertFileAtime("d1/subd", 801111, 0);
	assertFileMtime("d1/subd", 901111, 0);
	assertFileAtime("d1/subd/file2", 801100, 0);
	assertFileMtime("d1/subd/file2", 901100, 0);
}
