/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
__FBSDID("$FreeBSD: head/lib/libtransform/test/test_read_format_tbz.c 201247 2009-12-30 05:59:21Z kientzle $");

static unsigned char transform[] = {
'B','Z','h','9','1','A','Y','&','S','Y',237,7,140,'W',0,0,27,251,144,208,
128,0,' ','@',1,'o',128,0,0,224,'"',30,0,0,'@',0,8,' ',0,'T','2',26,163,'&',
129,160,211,212,18,'I',169,234,13,168,26,6,150,'1',155,134,'p',8,173,3,183,
'J','S',26,20,'2',222,'b',240,160,'a','>',205,'f',29,170,227,'[',179,139,
'\'','L','o',211,':',178,'0',162,134,'*','>','8',24,153,230,147,'R','?',23,
'r','E','8','P',144,237,7,140,'W'};

DEFINE_TEST(test_read_format_tbz)
{
	struct transform_entry *ae;
	struct transform *a;
	int r;

	assert((a = transform_read_new()) != NULL);
	r = transform_read_support_compression_bzip2(a);
	if (r != TRANSFORM_OK) {
		skipping("Bzip2 support");
		transform_read_free(a);
		return;
	}
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_format_all(a));
	assertEqualIntA(a, TRANSFORM_OK,
	    transform_read_open_memory(a, transform, sizeof(transform)));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_next_header(a, &ae));
	assertEqualInt(transform_compression(a), TRANSFORM_FILTER_BZIP2);
	assertEqualInt(transform_format(a), TRANSFORM_FORMAT_TAR_USTAR);
	assertEqualIntA(a, TRANSFORM_OK, transform_read_close(a));
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));
}


