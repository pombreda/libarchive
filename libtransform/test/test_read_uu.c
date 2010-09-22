/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2009 Michihiro NAKAJIMA
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
__FBSDID("$FreeBSD: head/lib/libtransform/test/test_read_uu.c 201248 2009-12-30 06:12:03Z kientzle $");

static const char extradata[] = {
"From uudecode@libtransform Mon Jun  2 03:03:31 2008\n"
"Return-Path: <uudecode@libtransform>\n"
"Received: from libtransform (localhost [127.0.0.1])\n"
"        by libtransform (8.14.2/8.14.2) with ESMTP id m5233UT1006448\n"
"        for <uudecode@libtransform>; Mon, 2 Jun 2008 03:03:31 GMT\n"
"        (envelope-from uudecode@libtransform)\n"
"Received: (from uudecode@localhost)\n"
"        by libtransform (8.14.2/8.14.2/Submit) id m5233U3e006406\n"
"        for uudecode; Mon, 2 Jun 2008 03:03:30 GMT\n"
"        (envelope-from root)\n"
"Date: Mon, 2 Jun 2008 03:03:30 GMT\n"
"From: Libtransform Test <uudecode@libtransform>\n"
"Message-Id: <200806020303.m5233U3e006406@libtransform>\n"
"To: uudecode@libtransform\n"
"Subject: Libtransform uudecode test\n"
"\n"
"* Redistribution and use in source and binary forms, with or without\n"
"* modification, are permitted provided that the following conditions\n"
"* are met:\n"
"\n"
"01234567890abcdeghijklmnopqrstuvwxyz\n"
"01234567890ABCEFGHIJKLMNOPQRSTUVWXYZ\n"
"\n"
};


static void
test_read_uu_sub(const char *uu_ref_file, const char *raw_ref_file)
{
	struct transform *a;
	char *buff = NULL;
	char *decoded_expected = NULL;
	char *uudata = NULL;
	int64_t data_len;
	int64_t uusize;
	int extra;

	assert(NULL != (decoded_expected = raw_read_reference_file(raw_ref_file,
		&data_len)));
	if(NULL == decoded_expected) {
		goto cleanup;
	}

	assert(NULL != (uudata = raw_read_reference_file(uu_ref_file,
		&uusize)));
	if(NULL == uudata) {
		goto cleanup;
	}

	assert(NULL != (buff = malloc(uusize + 64 * 1024)));
	if (buff == NULL) {
		goto cleanup;
	}


	for (extra = 0; extra <= 64; extra = extra==0?1:extra*2) {
		size_t size = extra * 1024;
		char *p = buff;

		failure_context("size(%d), extra(%d), uusize(%d)",
			size, extra, uusize);

		/* Add extra text size of which is from 1K bytes to
		 * 64Kbytes before uuencoded data. */
		while (size) {
			if (size > sizeof(extradata)-1) {
				memcpy(p, extradata, sizeof(extradata)-1);
				p += sizeof(extradata)-1;
				size -= sizeof(extradata)-1;
			} else {
				memcpy(p, extradata, size-1);
				p += size-1;
				*p++ = '\n';/* the last of extra text must have
					     * '\n' character. */
				break;
			}
		}
		memcpy(p, uudata, uusize);
		size = extra * 1024 + uusize;

		assert((a = transform_read_new()) != NULL);
		struct transform_read_bidder *trb;
		assert(NULL != (trb = transform_read_add_autodetect(a)));
		assertEqualInt(TRANSFORM_OK, transform_autodetect_add_uu(trb));
//		assertEqualIntA(a, TRANSFORM_OK,
//		    transform_read_add_uu(a));
		assertEqualIntA(a, TRANSFORM_OK,
		    read_open_memory(a, buff, size, 2));

		// ensure there is a uu filter in use.
		assertEqualInt(2, transform_filter_count(a));

		assertTransformContentsMem(a, (void *)decoded_expected, 
			(int64_t)data_len);
		
		assertEqualIntA(a, TRANSFORM_OK,
			transform_errno(a));
		// assert we're at the end of the file via this.
		transform_read_consume(a, 1);
		assertEqualIntA(a, TRANSFORM_EOF,
			transform_errno(a));


		failure("transform_filter_name(a, 0)=\"%s\"",
		    transform_filter_name(a, 0));
		assertEqualInt(transform_filter_code(a, 0),
		    TRANSFORM_FILTER_UU);
		assertEqualIntA(a, TRANSFORM_OK, transform_read_close(a));
		assertEqualInt(TRANSFORM_OK, transform_read_free(a));
	}

cleanup:

	failure_context(NULL);

	if(buff)
		free(buff);
	if(decoded_expected)
		free(decoded_expected);
	if(uudata)
		free(uudata);
}

DEFINE_TEST(test_read_uu)
{
	/* Read the traditional uuencoded data. */
	test_read_uu_sub("test_read_uu.data.uu", "test_read_uu.data.raw");
	/* Read the Base64 uuencoded data. */
	test_read_uu_sub("test_read_uu.data64.uu", "test_read_uu.data64.raw");
}

