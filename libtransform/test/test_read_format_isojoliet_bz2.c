/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Based on libtransform/test/test_read_format_isorr_bz2.c with
 * bugs introduced by Andreas Henriksson <andreas@fatal.se> for
 * testing ISO9660 image with Joliet extension.
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
__FBSDID("$FreeBSD: head/lib/libtransform/test/test_read_format_isojoliet_bz2.c 201247 2009-12-30 05:59:21Z kientzle $");

/*
Execute the following to rebuild the data for this program:
   tail -n +35 test_read_format_isojoliet_bz2.c | /bin/sh

rm -rf /tmp/iso
mkdir /tmp/iso
mkdir /tmp/iso/dir
echo "hello" >/tmp/iso/long-joliet-file-name.textfile
ln /tmp/iso/long-joliet-file-name.textfile /tmp/iso/hardlink
(cd /tmp/iso; ln -s long-joliet-file-name.textfile symlink)
if [ "$(uname -s)" = "Linux" ]; then # gnu coreutils touch doesn't have -h
TZ=utc touch -afm -t 197001020000.01 /tmp/iso /tmp/iso/long-joliet-file-name.textfile /tmp/iso/dir
TZ=utc touch -afm -t 197001030000.02 /tmp/iso/symlink
else
TZ=utc touch -afhm -t 197001020000.01 /tmp/iso /tmp/iso/long-joliet-file-name.textfile /tmp/iso/dir
TZ=utc touch -afhm -t 197001030000.02 /tmp/iso/symlink
fi
F=test_read_format_iso_joliet.iso.Z
mkhybrid -J -uid 1 -gid 2 /tmp/iso | compress > $F
uuencode $F $F > $F.uu
exit 1
 */

DEFINE_TEST(test_read_format_isojoliet_bz2)
{
	const char *refname = "test_read_format_iso_joliet.iso.Z";
	struct transform_entry *ae;
	struct transform *a;
	const void *p;
	size_t size;
	int64_t offset;

	extract_reference_file(refname);
	assert((a = transform_read_new()) != NULL);
	assertEqualInt(0, transform_read_support_compression_all(a));
	assertEqualInt(0, transform_read_support_format_all(a));
	assertEqualInt(TRANSFORM_OK,
	    transform_read_set_options(a, "iso9660:!rockridge"));
	assertEqualInt(TRANSFORM_OK,
	    transform_read_open_filename(a, refname, 10240));

	/* First entry is '.' root directory. */
	assertEqualInt(0, transform_read_next_header(a, &ae));
	assertEqualString(".", transform_entry_pathname(ae));
	assertEqualInt(AE_IFDIR, transform_entry_filetype(ae));
	assertEqualInt(2048, transform_entry_size(ae));
	assertEqualInt(86401, transform_entry_mtime(ae));
	assertEqualInt(0, transform_entry_mtime_nsec(ae));
	assertEqualInt(86401, transform_entry_ctime(ae));
	assertEqualInt(3, transform_entry_stat(ae)->st_nlink);
	assertEqualInt(0, transform_entry_uid(ae));
	assertEqualIntA(a, TRANSFORM_EOF,
	    transform_read_data_block(a, &p, &size, &offset));
	assertEqualInt((int)size, 0);

	/* A directory. */
	assertEqualInt(0, transform_read_next_header(a, &ae));
	assertEqualString("dir", transform_entry_pathname(ae));
	assertEqualInt(AE_IFDIR, transform_entry_filetype(ae));
	assertEqualInt(2048, transform_entry_size(ae));
	assertEqualInt(86401, transform_entry_mtime(ae));
	assertEqualInt(86401, transform_entry_atime(ae));

	/* A regular file with two names ("hardlink" gets returned
	 * first, so it's not marked as a hardlink). */
	assertEqualInt(0, transform_read_next_header(a, &ae));
	assertEqualString("long-joliet-file-name.textfile",
	    transform_entry_pathname(ae));
	assertEqualInt(AE_IFREG, transform_entry_filetype(ae));
	assert(transform_entry_hardlink(ae) == NULL);
	assertEqualInt(6, transform_entry_size(ae));
	assertEqualInt(0, transform_read_data_block(a, &p, &size, &offset));
	assertEqualInt(6, (int)size);
	assertEqualInt(0, offset);
	assertEqualInt(0, memcmp(p, "hello\n", 6));

	/* Second name for the same regular file (this happens to be
	 * returned second, so does get marked as a hardlink). */
	assertEqualInt(0, transform_read_next_header(a, &ae));
	assertEqualString("hardlink", transform_entry_pathname(ae));
	assertEqualInt(AE_IFREG, transform_entry_filetype(ae));
	assertEqualString("long-joliet-file-name.textfile",
	    transform_entry_hardlink(ae));
	assert(!transform_entry_size_is_set(ae));

	/* A symlink to the regular file. */
	assertEqualInt(0, transform_read_next_header(a, &ae));
	assertEqualString("symlink", transform_entry_pathname(ae));
	assertEqualInt(0, transform_entry_size(ae));
	assertEqualInt(172802, transform_entry_mtime(ae));
	assertEqualInt(172802, transform_entry_atime(ae));

	/* End of transform. */
	assertEqualInt(TRANSFORM_EOF, transform_read_next_header(a, &ae));

	/* Verify transform format. */
	assertEqualInt(transform_compression(a), TRANSFORM_FILTER_COMPRESS);

	/* Close the transform. */
	assertEqualIntA(a, TRANSFORM_OK, transform_read_close(a));
	assertEqualInt(TRANSFORM_OK, transform_read_free(a));
}

