/*-
 * Copyright (c) 2003-2009 Tim Kientzle
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
__FBSDID("$FreeBSD: head/lib/libtransform/test/test_read_file_nonexistent.c 189473 2009-03-07 02:09:21Z kientzle $");

void
read_test(const char *name)
{
	struct transform* a = transform_read_new();
	if(TRANSFORM_OK != transform_read_support_compression_bzip2(a)) {
		skipping("bzip2 unsupported");
		return;
	}

	extract_reference_file(name);
	assertEqualIntA(a, TRANSFORM_OK, transform_read_open_filename(a, name, 2));
	/* bzip2 and none */
	assertEqualInt(2, transform_filter_count(a));
	
	transform_read_free(a);
}

void
write_test(void)
{
	char buff[4096];
	struct transform* a = transform_write_new();

	if(TRANSFORM_OK != transform_write_add_filter_bzip2(a)) {
		skipping("bzip2 unsupported");
		return;
	}
	assertEqualIntA(a, TRANSFORM_OK, transform_write_open_memory(a, buff, 4096, 0));
	/* bzip2 and none */
	assertEqualInt(2, transform_filter_count(a));
	transform_write_free(a);
}

DEFINE_TEST(test_filter_count)
{
	read_test("test_compat_bzip2_1.tbz");
	write_test();
}


