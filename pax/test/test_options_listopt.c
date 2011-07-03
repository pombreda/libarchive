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
#define setenv(_name, _value, f)	_putenv_s(_name, _value)
#endif

DEFINE_TEST(test_options_listopt)
{
	const char *reffile1 = "test_options_listopt.pax.Z";

	extract_reference_file(reffile1);

	/* Test1: without -v option. */
	assertEqualInt(0, systemf(
	    "%s < %s >test.out 2>test.err", testprog, reffile1));

	failure("Output should be only filenames");
	assertTextFileContents("file1\nhardlink1\noldfile\nsymlink1\n",
	    "test.out");
	assertTextFileContents(
	    "bsdpax: POSIX pax interchange format,"
	    " compress (.Z) filters, 4 files,"
	    " 8192 bytes read, 0 bytes written\n",
	    "test.err");

	setenv("LANG", "C", 1);

	/* Test2: with -v option. */
	assertEqualInt(0, systemf(
	    "%s -v < %s >test.out 2>test.err", testprog, reffile1));

	failure("Output should be default value");
	assertTextFileContents(
"-rw-r--r--  0 cue    cue        17 Jun 23 14:07 file1\n"
"hrw-r--r--  0 cue    cue         0 Jun 23 14:07 hardlink1 link to file1\n"
"-rw-r--r--  0 cue    cue         4 Jan  1  1980 oldfile\n"
"lrwxr-xr-x  0 cue    cue         0 Jun 23 14:08 symlink1 -> file1\n",
	    "test.out");
	assertTextFileContents(
	    "bsdpax: POSIX pax interchange format,"
	    " compress (.Z) filters, 4 files,"
	    " 8192 bytes read, 0 bytes written\n",
	    "test.err");

	/* Test3: with listopt.
	 * Make bsdpax print only filenames. */
	assertEqualInt(0, systemf(
	    "%s -o listopt=%%F < %s >test.out 2>test.err",
	    testprog, reffile1));

	failure("Output should be only a filename");
	assertTextFileContents("file1\nhardlink1\noldfile\nsymlink1\n",
	    "test.out");
	assertTextFileContents(
	    "bsdpax: POSIX pax interchange format,"
	    " compress (.Z) filters, 4 files,"
	    " 8192 bytes read, 0 bytes written\n",
	    "test.err");

	/* Test4: with listopt.
	 * Make bsdpax print a mode and a filename. */
	assertEqualInt(0, systemf(
	    "%s -o listopt=%%M%%F < %s >test.out 2>test.err",
	    testprog, reffile1));

	failure("Output should be only filenames");
	assertTextFileContents(
	    "-rw-r--r-- file1\n"
	    "hrw-r--r-- hardlink1\n"
	    "-rw-r--r-- oldfile\n"
	    "lrwxr-xr-x symlink1\n",
	    "test.out");
	assertTextFileContents(
	    "bsdpax: POSIX pax interchange format,"
	    " compress (.Z) filters, 4 files,"
	    " 8192 bytes read, 0 bytes written\n",
	    "test.err");

	/* Test5: with listopt.
	 * Make bsdpax print a date and a filename. */
	assertEqualInt(0, systemf(
	    "%s -o listopt=%%T_%%F < %s >test.out 2>test.err",
	    testprog, reffile1));

	failure("Output should be only filenames");
	assertTextFileContents(
	    "Jun 23 14:07_file1\n"
	    "Jun 23 14:07_hardlink1\n"
	    "Jan  1  1980_oldfile\n"
	    "Jun 23 14:08_symlink1\n",
	    "test.out");
	assertTextFileContents(
	    "bsdpax: POSIX pax interchange format,"
	    " compress (.Z) filters, 4 files,"
	    " 8192 bytes read, 0 bytes written\n",
	    "test.err");

	/* Test6: with listopt.
	 * Make bsdpax print a date specified atime, and a filename . */
	assertEqualInt(0, systemf(
	    "%s -o listopt=%%\\(atime\\)T_%%F"
	    " < %s >test.out 2>test.err",
	    testprog, reffile1));

	failure("Output should be only filenames");
	assertTextFileContents(
	    "Jun 26 06:30_file1\n"
	    "Jun 26 06:32_hardlink1\n"
	    "Jun 26 06:30_oldfile\n"
	    "Jun 23 14:08_symlink1\n",
	    "test.out");
	assertTextFileContents(
	    "bsdpax: POSIX pax interchange format,"
	    " compress (.Z) filters, 4 files,"
	    " 8192 bytes read, 0 bytes written\n",
	    "test.err");

	/* Test7: with listopt.
	 * Make bsdpax print a date specified ctime, and a filename . */
	assertEqualInt(0, systemf(
	    "%s -o listopt=%%\\(ctime\\)T_%%F"
	    " < %s >test.out 2>test.err",
	    testprog, reffile1));

	failure("Output should be only filenames");
	assertTextFileContents(
	    "Jun 23 14:08_file1\n"
	    "Jun 23 14:08_hardlink1\n"
	    "Jun 23 14:09_oldfile\n"
	    "Jun 23 14:08_symlink1\n",
	    "test.out");
	assertTextFileContents(
	    "bsdpax: POSIX pax interchange format,"
	    " compress (.Z) filters, 4 files,"
	    " 8192 bytes read, 0 bytes written\n",
	    "test.err");

	/* Test8: with listopt.
	 * Make bsdpax print a date specified mtime, and a filename . */
	assertEqualInt(0, systemf(
	    "%s -o listopt=%%\\(mtime\\)T_%%F"
	    " < %s >test.out 2>test.err",
	    testprog, reffile1));

	failure("Output should be only filenames");
	assertTextFileContents(
	    "Jun 23 14:07_file1\n"
	    "Jun 23 14:07_hardlink1\n"
	    "Jan  1  1980_oldfile\n"
	    "Jun 23 14:08_symlink1\n",
	    "test.out");
	assertTextFileContents(
	    "bsdpax: POSIX pax interchange format,"
	    " compress (.Z) filters, 4 files,"
	    " 8192 bytes read, 0 bytes written\n",
	    "test.err");

	/* Test9: with listopt.
	 * Make bsdpax print a date specified its time format
	 * like "yyyy-mm-dd", and a filename . */
	assertEqualInt(0, systemf(
	    "%s -o listopt=%%\\(mtime=%%Y-%%m-%%d\\)T_%%F"
	    " < %s >test.out 2>test.err",
	    testprog, reffile1));

	failure("Output should be only filenames");
	assertTextFileContents(
	    "2011-06-23_file1\n"
	    "2011-06-23_hardlink1\n"
	    "1980-01-01_oldfile\n"
	    "2011-06-23_symlink1\n",
	    "test.out");
	assertTextFileContents(
	    "bsdpax: POSIX pax interchange format,"
	    " compress (.Z) filters, 4 files,"
	    " 8192 bytes read, 0 bytes written\n",
	    "test.err");

	/* Test10: with listopt.
	 * Make bsdpax print a date specified its time format
	 * like "HH:MM", and a filename . */
	assertEqualInt(0, systemf(
	    "%s -o listopt=%%\\(mtime=%%H:%%M\\)T_%%F"
	    " < %s >test.out 2>test.err",
	    testprog, reffile1));

	failure("Output should be only filenames");
	assertTextFileContents(
	    "14:07_file1\n"
	    "14:07_hardlink1\n"
	    "01:01_oldfile\n"
	    "14:08_symlink1\n",
	    "test.out");
	assertTextFileContents(
	    "bsdpax: POSIX pax interchange format,"
	    " compress (.Z) filters, 4 files,"
	    " 8192 bytes read, 0 bytes written\n",
	    "test.err");

	/* Test11: with listopt.
	 * Make bsdpax print a filename and a linkname. */
	assertEqualInt(0, systemf(
	    "%s -o listopt=%%L < %s >test.out 2>test.err",
	    testprog, reffile1));

	failure("Output should be only a filename");
	assertTextFileContents(
	    "file1\n"
	    "hardlink1 link to file1\n"
	    "oldfile\n"
	    "symlink1 -> file1\n",
	    "test.out");
	assertTextFileContents(
	    "bsdpax: POSIX pax interchange format,"
	    " compress (.Z) filters, 4 files,"
	    " 8192 bytes read, 0 bytes written\n",
	    "test.err");

	/* Test12: with listopt.
	 * Make bsdpax print a file size and a filename. */
	assertEqualInt(0, systemf(
	    "%s -o listopt=%%4\\(size\\)u%%10F"
	    " < %s >test.out 2>test.err",
	    testprog, reffile1));

	failure("Output should be only a filename");
	assertTextFileContents(
	    "  17     file1\n"
	    "   0 hardlink1\n"
	    "   4   oldfile\n"
	    "   0  symlink1\n",
	    "test.out");
	assertTextFileContents(
	    "bsdpax: POSIX pax interchange format,"
	    " compress (.Z) filters, 4 files,"
	    " 8192 bytes read, 0 bytes written\n",
	    "test.err");

	/* Test13: with listopt.
	 * Make bsdpax print a file size with zero-padding
	 * and a filename in left alignment. */
	assertEqualInt(0, systemf(
	    "%s -o listopt=%%04\\(size\\)u%%-10F"
	    " < %s >test.out 2>test.err",
	    testprog, reffile1));

	failure("Output should be only a filename");
	assertTextFileContents(
	    "0017file1     \n"
	    "0000hardlink1 \n"
	    "0004oldfile   \n"
	    "0000symlink1  \n",
	    "test.out");
	assertTextFileContents(
	    "bsdpax: POSIX pax interchange format,"
	    " compress (.Z) filters, 4 files,"
	    " 8192 bytes read, 0 bytes written\n",
	    "test.err");

	/* Test14: with listopt.
	 * Make bsdpax print a file size in hex and a mode in oct
	 * and a filename. */
	assertEqualInt(0, systemf(
	    "%s -o listopt=%%04\\(size\\)x%%7\\(mode\\)o%%10F"
	    " < %s >test.out 2>test.err",
	    testprog, reffile1));

	failure("Output should be only a filename");
	assertTextFileContents(
	    "0011 100644     file1\n"
	    "0000    644 hardlink1\n"
	    "0004 100644   oldfile\n"
	    "0000 120755  symlink1\n",
	    "test.out");
	assertTextFileContents(
	    "bsdpax: POSIX pax interchange format,"
	    " compress (.Z) filters, 4 files,"
	    " 8192 bytes read, 0 bytes written\n",
	    "test.err");
}
