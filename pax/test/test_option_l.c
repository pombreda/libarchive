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

DEFINE_TEST(test_option_l)
{
	assertMakeDir("d1", 0755);
	assertMakeFile("d1/file1", 0644, "d1/file1");
	if (canSymlink()) {
		assertChdir("d1");
		assertMakeSymlink("lf1", "file1");
		assertMakeSymlink("lfx", "filex");/* broken link */
		assertChdir("..");
	}

	/* Test 1: without -l */
	assertMakeDir("dest1", 0755);
	assertEqualInt(0, systemf("%s -rw d1 dest1 >dest1/c.out 2>dest1/c.err",
	    testprog));
	assertChdir("dest1");
	assertEmptyFile("c.out");
	assertEmptyFile("c.err");
	assertIsDir("d1", 0755);
	assertIsReg("d1/file1", 0644);
	assertFileContents("d1/file1", 8, "d1/file1");
	if (canSymlink()) {
		assertIsSymlink("d1/lf1", "file1");
		assertIsSymlink("d1/lfx", "filex");
	}
	assertChdir("..");
	assertIsNotHardlink("d1/file1", "dest1/d1/file1");

	/* Test 1: with -l */
	assertMakeDir("dest2", 0755);
	assertEqualInt(0, systemf("%s -rw -l d1 dest2 >dest2/c.out 2>dest2/c.err",
	    testprog));
	assertChdir("dest2");
	assertEmptyFile("c.out");
	assertEmptyFile("c.err");
	assertIsDir("d1", 0755);
	assertIsReg("d1/file1", 0644);
	assertFileContents("d1/file1", 8, "d1/file1");
	if (canSymlink()) {
		assertIsSymlink("d1/lf1", "file1");
		assertIsSymlink("d1/lfx", "filex");
	}
	assertChdir("..");
	assertIsHardlink("d1/file1", "dest2/d1/file1");
}
