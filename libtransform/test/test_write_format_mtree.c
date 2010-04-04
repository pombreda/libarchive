/*-
 * Copyright (c) 2009 Michihiro NAKAJIMA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
__FBSDID("$FreeBSD: head/lib/libtransform/test/test_write_format_mtree.c 191183 2009-04-17 01:06:31Z kientzle $");

static char buff[4096];
static struct {
	const char	*path;
	mode_t		 mode;
	time_t		 mtime;
	uid_t	 	 uid;
	gid_t		 gid;
} entries[] = {
	{ "./Makefile", 	S_IFREG | 0644, 1233041050, 1001, 1001 },
	{ "./NEWS", 		S_IFREG | 0644, 1231975636, 1001, 1001 },
	{ "./PROJECTS", 	S_IFREG | 0644, 1231975636, 1001, 1001 },
	{ "./README",		S_IFREG | 0644, 1231975636, 1001, 1001 },
	{ "./COPYING",		S_IFREG | 0644, 1231975636, 1001, 1001 },
	{ "./subdir",		S_IFDIR | 0755, 1233504586, 1001, 1001 },
	{ "./subdir/README",	S_IFREG | 0664, 1231975636, 1002, 1001 },
	{ "./subdir/config",	S_IFREG | 0664, 1232266273, 1003, 1003 },
	{ "./subdir2",		S_IFDIR | 0755, 1233504586, 1001, 1001 },
	{ "./subdir3",		S_IFDIR | 0755, 1233504586, 1001, 1001 },
	{ "./subdir3/mtree",	S_IFREG | 0664, 1232266273, 1003, 1003 },
	{ NULL, 0, 0, 0, 0 }
};

static void
test_write_format_mtree_sub(int use_set, int dironly)
{
	struct transform_entry *ae;
	struct transform* a;
	size_t used;
	int i;

	/* Create a mtree format transform. */
	assert((a = transform_write_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_write_set_format_mtree(a));
	if (use_set)
		assertEqualIntA(a, TRANSFORM_OK, transform_write_set_options(a, "use-set"));
	if (dironly)
		assertEqualIntA(a, TRANSFORM_OK, transform_write_set_options(a, "dironly"));
	assertEqualIntA(a, TRANSFORM_OK, transform_write_open_memory(a, buff, sizeof(buff)-1, &used));

	/* Write entries */
	for (i = 0; entries[i].path != NULL; i++) {
		assert((ae = transform_entry_new()) != NULL);
		transform_entry_set_mtime(ae, entries[i].mtime, 0);
		assert(entries[i].mtime == transform_entry_mtime(ae));
		transform_entry_set_mode(ae, entries[i].mode);
		assert(entries[i].mode == transform_entry_mode(ae));
		transform_entry_set_uid(ae, entries[i].uid);
		assert(entries[i].uid == transform_entry_uid(ae));
		transform_entry_set_gid(ae, entries[i].gid);
		assert(entries[i].gid == transform_entry_gid(ae));
		transform_entry_copy_pathname(ae, entries[i].path);
		if ((entries[i].mode & AE_IFMT) != S_IFDIR)
			transform_entry_set_size(ae, 8);
		assertEqualIntA(a, TRANSFORM_OK, transform_write_header(a, ae));
		if ((entries[i].mode & AE_IFMT) != S_IFDIR)
			assertEqualIntA(a, 8,
			    transform_write_data(a, "Hello012", 15));
		transform_entry_free(ae);
	}
	assertEqualIntA(a, TRANSFORM_OK, transform_write_close(a));
        assertEqualInt(TRANSFORM_OK, transform_write_free(a));

	if (use_set) {
		const char *p;

		buff[used] = '\0';
		assert(NULL != (p = strstr(buff, "\n/set ")));
		if (p != NULL) {
			char *r;
			const char *o;
			p++;
			r = strchr(p, '\n');
			if (r != NULL)
				*r = '\0';
			if (dironly)
				o = "/set type=dir uid=1001 gid=1001 mode=755";
			else
				o = "/set type=file uid=1001 gid=1001 mode=644";
			assertEqualString(o, p);
			if (r != NULL)
				*r = '\n';
		}
	}

	/*
	 * Read the data and check it.
	 */
	assert((a = transform_read_new()) != NULL);
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_format_all(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_support_compression_all(a));
	assertEqualIntA(a, TRANSFORM_OK, transform_read_open_memory(a, buff, used));

	/* Read entries */
	for (i = 0; entries[i].path != NULL; i++) {
		if (dironly && (entries[i].mode & AE_IFMT) != S_IFDIR)
			continue;
		assertEqualIntA(a, TRANSFORM_OK, transform_read_next_header(a, &ae));
		assertEqualInt(entries[i].mtime, transform_entry_mtime(ae));
		assertEqualInt(entries[i].mode, transform_entry_mode(ae));
		assertEqualInt(entries[i].uid, transform_entry_uid(ae));
		assertEqualInt(entries[i].gid, transform_entry_gid(ae));
		assertEqualString(entries[i].path, transform_entry_pathname(ae));
		if ((entries[i].mode & AE_IFMT) != S_IFDIR)
			assertEqualInt(8, transform_entry_size(ae));
	}
	assertEqualIntA(a, TRANSFORM_OK, transform_read_close(a));
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));
}

DEFINE_TEST(test_write_format_mtree)
{
	/* Default setting */
	test_write_format_mtree_sub(0, 0);
	/* Directory only */
	test_write_format_mtree_sub(0, 1);
	/* Use /set keyword */
	test_write_format_mtree_sub(1, 0);
	/* Use /set keyword with directory only */
	test_write_format_mtree_sub(1, 1);
}
