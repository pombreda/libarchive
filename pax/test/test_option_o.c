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

#if defined(_WIN32) && !defined(__CYGWIN__)
#define BSDPAX	"bsdpax.exe"
#define LARC	"("
#define RARC	")"
#else
#define BSDPAX	"bsdpax"
#define LARC	"\\("
#define RARC	"\\)"
#endif

DEFINE_TEST(test_option_o)
{

	assertMakeFile("file1", 0644, "contents of file1");

	/* Test0: Make an archive with explicit file attributes. */
	assertEqualInt(0,
	    systemf("%s --format pax -wf archive.pax "
	 	" -o uid=1010,uname=testor1,gid=6554,gname=test,mode=0666,"
		"mtime=1999/09/30"
		" file1 > test0.out 2>test0.err", testprog));
	assertEmptyFile("test0.out");
	assertEmptyFile("test0.err");
	
	/* Test1: Check the explicit file attributes. */
	assertEqualInt(0,
	    systemf("%s -vf archive.pax "
	 	" -o listopt=%%5" LARC "uid" RARC "d:%%8"
		LARC "uname" RARC "s:%%5" LARC "gid" RARC "d:"
		"%%8" LARC "gname" RARC "s:%%" LARC "mode" RARC "o:"
		"%%" LARC "mtime=%%Y-%%m-%%d" RARC "T"
		" > test1.out 2>test1.err", testprog));
	assertTextFileContents(
	    " 1010: testor1: 6554:    test:100666:1999-09-30\n",
	    "test1.out");
	assertTextFileContents(
	    BSDPAX ": POSIX pax interchange format,"
	    " 1 files,"
	    " 3072 bytes read, 0 bytes written\n",
	    "test1.err");

	/* Test2: Clone an archive with explicit file attributes. */
	assertEqualInt(0,
	    systemf("%s --format pax -wf archive2.pax "
	 	" -o uid=1021,uname=user99,gid=777,gname=user,mode=0600,"
		"mtime=2004/11/21"
		" @archive.pax > test2.out 2>test2.err", testprog));
	assertEmptyFile("test2.out");
	assertEmptyFile("test2.err");

	/* Test3: Check the explicit file attributes. */
	assertEqualInt(0,
	    systemf("%s -vf archive2.pax "
	 	" -o listopt=%%5" LARC "uid" RARC "d:%%8"
		LARC "uname" RARC "s:%%5" LARC "gid" RARC "d:"
		"%%8" LARC "gname" RARC "s:%%" LARC "mode" RARC "o:"
		"%%" LARC "mtime=%%Y-%%m-%%d" RARC "T"
		" > test1.out 2>test1.err", testprog));
	assertTextFileContents(
	    " 1021:  user99:  777:    user:100600:2004-11-21\n",
	    "test1.out");
	assertTextFileContents(
	    BSDPAX ": POSIX pax interchange format,"
	    " 1 files,"
	    " 3072 bytes read, 0 bytes written\n",
	    "test1.err");

	/* Test4: Extract an archive with explicit file attributes. */
	assertMakeDir("test4", 0755);
	assertChdir("test4");
	assertEqualInt(0,
	    systemf("%s -vrf ../archive.pax "
	 	" -o mtime=2009/09/21,atime=2010/09/22"
		" > test4.out 2>test4.err", testprog));
	assertEmptyFile("test4.out");
	assertTextFileContents(
	    "file1\n" BSDPAX ": POSIX pax interchange format,"
	    " 1 files,"
	    " 3072 bytes read, 0 bytes written\n",
	    "test4.err");
	assertFileAtime("file1", 1285081200, 0);
	assertFileMtime("file1", 1253458800, 0);
	assertChdir("..");
}
