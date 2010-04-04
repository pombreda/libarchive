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
__FBSDID("$FreeBSD$");

#define UID	1001
#define UNAME	"cue"
#define GID	1001
#define GNAME	"cue"

/* Verify that a file records with hardlink.
#How to make
echo "hellohellohello" > f1
chown $UNAME:$GNAME f1
chmod 0644 f1
ln f1 hardlink
chown $UNAME:$GNAME hardlink
chmod 0644 hardlink
env TZ=utc touch -afm -t 197001020000.01 f1 hardlink
xar -cf archive1.xar f1 hardlink
od -t x1 archive1.xar | sed -E -e 's/^0[0-9]+//;s/^  //;s/(  )([0-9a-f]{2})/0x\2,/g;$ D' >  archive1.xar.txt
*/
static unsigned char archive1[] = {
0x78,0x61,0x72,0x21,0x00,0x1c,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xc6,
0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x70,0x00,0x00,0x00,0x01,0x78,0xda,0xc4,0x54,
0xc9,0x6e,0xdb,0x30,0x14,0xbc,0xe7,0x2b,0x08,0xdd,0x55,0xae,0xb6,0x45,0x83,0x56,
0xd0,0x4b,0xd1,0x7b,0xd3,0x4b,0x6f,0x34,0x17,0x89,0x88,0x36,0x48,0x54,0xe0,0xe4,
0xeb,0x4b,0x52,0x52,0x0c,0xa7,0x71,0x6f,0x45,0x01,0x01,0x1a,0x0e,0x87,0xa3,0xa7,
0xf7,0x06,0x14,0x8f,0x97,0xb6,0x01,0x2f,0x66,0x9c,0x5c,0xdf,0x9d,0x32,0xfc,0x05,
0x65,0xc0,0x74,0xaa,0xd7,0xae,0xab,0x4e,0xd9,0xcf,0xa7,0x6f,0x79,0x91,0x3d,0x96,
0x0f,0xe2,0x22,0xc7,0xf2,0x01,0x08,0xdf,0xab,0xf0,0x02,0x42,0x8d,0x46,0xfa,0x70,
0x22,0xf7,0xae,0x35,0x25,0x41,0x88,0xe7,0x98,0xe4,0x88,0x3c,0x61,0x7a,0xa4,0xe8,
0x48,0xb9,0x80,0xb7,0x92,0x74,0xa8,0x36,0xea,0x79,0x9a,0x5b,0x30,0xf9,0xd7,0xc6,
0x9c,0xb2,0xa9,0x96,0x38,0x8b,0x3b,0x40,0xf4,0xd6,0x4e,0xc6,0x97,0x48,0xc0,0x15,
0x25,0x76,0x72,0x6f,0xd1,0x5c,0xc0,0x04,0xa2,0x05,0xdc,0x3c,0xd2,0xca,0xba,0xc6,
0x00,0xa7,0x4f,0x19,0x59,0x6d,0xd4,0x9d,0x72,0xd8,0xaf,0x70,0x72,0xab,0x03,0x88,
0x36,0x41,0xcc,0x0f,0x28,0x47,0x38,0xca,0x10,0x3a,0xc6,0x07,0x07,0x59,0x7b,0x95,
0xc9,0x7b,0x3f,0x17,0x64,0xf2,0x2a,0xab,0xc6,0x7e,0x1e,0x4a,0x35,0x1b,0x01,0x17,
0xb8,0xb0,0x4e,0x97,0x18,0x21,0x1c,0xc8,0x80,0x12,0x35,0x4f,0x66,0x5c,0x74,0x09,
0x2d,0xdc,0xbb,0x6c,0xde,0x64,0x6d,0xaf,0x4d,0x89,0xf6,0x8c,0x85,0x62,0x22,0x4c,
0xa4,0x7f,0x1d,0x0c,0x68,0x5c,0xf7,0x1c,0x66,0x94,0x95,0xb5,0x1c,0x75,0x5c,0x08,
0x18,0xf9,0x45,0xd1,0xc9,0x50,0xd0,0x75,0x23,0x2d,0x53,0xcb,0x62,0x97,0x6e,0xdb,
0xb5,0x75,0x5d,0x4b,0x2f,0x13,0x02,0xa2,0x31,0x5d,0xe5,0xeb,0x92,0x50,0x01,0x57,
0xb8,0xf0,0xeb,0x38,0xc8,0xed,0x64,0xd6,0xd1,0xe0,0xfd,0x75,0x34,0x81,0xdb,0x72,
0xb3,0xcd,0x57,0x0e,0x43,0xe3,0x54,0x0a,0x01,0xbc,0xe4,0xd5,0x9b,0x1b,0x32,0xb8,
0x4a,0xe5,0xa8,0x6a,0xf7,0x62,0x74,0xfe,0x31,0x13,0x3f,0xbe,0x7f,0x0d,0xd5,0xd9,
0x82,0x52,0x4d,0xac,0x56,0x98,0x53,0xc6,0xa9,0x3c,0xb3,0x82,0x4b,0x2d,0x09,0xb5,
0x85,0x3d,0x70,0x6c,0xf7,0xc4,0x2a,0xba,0xe7,0x45,0x98,0xc3,0x47,0xa3,0xad,0x96,
0x8b,0x1f,0xa5,0xf2,0x77,0xbf,0xb0,0xd3,0x07,0x76,0x56,0x67,0x75,0xe0,0x9a,0x5a,
0x7e,0xb6,0x4c,0xda,0xe0,0xcd,0x8a,0xa2,0x40,0x86,0xed,0xc8,0x7e,0xc7,0xac,0x41,
0x8a,0x87,0x1c,0xff,0xe9,0xb4,0x34,0x0f,0xbe,0x77,0xef,0x9f,0xc4,0xee,0x73,0xd9,
0x7f,0x8c,0x5d,0x3f,0xba,0xca,0x75,0xb2,0xf9,0x4b,0xfa,0x2c,0xfe,0x24,0x77,0x41,
0x15,0x2f,0x0d,0x01,0xd3,0x15,0xf2,0x1b,0x00,0x00,0xff,0xff,0x03,0x00,0x88,0x32,
0x49,0x7b,0x67,0xbf,0xc6,0x01,0x29,0xf2,0x1c,0x40,0x05,0x3c,0x49,0x25,0x9f,0xab,
0x7c,0x8e,0xc5,0xa5,0x79,0xe0,0x78,0xda,0xca,0x48,0xcd,0xc9,0xc9,0xcf,0x80,0x13,
0x5c,0x00,0x00,0x00,0x00,0xff,0xff,0x03,0x00,0x37,0xf7,0x06,0x47
};

static void verify0(struct transform *a, struct transform_entry *ae)
{
	const void *p;
	size_t size;
#if ARCHIVE_VERSION_NUMBER < 3000000
	off_t offset;
#else
	int64_t offset;
#endif

	assert(archive_entry_filetype(ae) == AE_IFREG);
	assertEqualInt(archive_entry_mode(ae) & 0777, 0644);
	assertEqualInt(archive_entry_uid(ae), UID);
	assertEqualInt(archive_entry_gid(ae), GID);
	assertEqualString(archive_entry_uname(ae), UNAME);
	assertEqualString(archive_entry_gname(ae), GNAME);
	assertEqualString(archive_entry_pathname(ae), "f1");
	assert(archive_entry_hardlink(ae) == NULL);
	assert(archive_entry_symlink(ae) == NULL);
	assertEqualInt(archive_entry_mtime(ae), 86401);
	assertEqualInt(archive_entry_size(ae), 16);
	assertEqualInt(transform_read_data_block(a, &p, &size, &offset), 0);
	assertEqualInt((int)size, 16);
	assertEqualInt((int)offset, 0);
	assertEqualInt(memcmp(p, "hellohellohello\n", 16), 0);
}

static void verify1(struct transform *a, struct transform_entry *ae)
{
	(void)a; /* UNUSED */
	/* A hardlink is not a symlink. */
	assert(archive_entry_filetype(ae) != AE_IFLNK);
	/* Nor is it a directory. */
	assert(archive_entry_filetype(ae) != AE_IFDIR);
	assertEqualInt(archive_entry_mode(ae) & 0777, 0644);
	assertEqualInt(archive_entry_uid(ae), UID);
	assertEqualInt(archive_entry_gid(ae), GID);
	assertEqualString(archive_entry_uname(ae), UNAME);
	assertEqualString(archive_entry_gname(ae), GNAME);
	assertEqualString(archive_entry_pathname(ae), "hardlink");
	assertEqualString(archive_entry_hardlink(ae), "f1");
	assert(archive_entry_symlink(ae) == NULL);
	assertEqualInt(archive_entry_mtime(ae), 86401);
	assertEqualInt(archive_entry_nlink(ae), 2);
}

/* Verify that symlinks are read correctly.
#How to make
echo "hellohellohello" > f1
chown $UNAME:$GNAME f1
chmod 0644 f1
ln -s f1 symlink
chown $UNAME:$GNAME symlink
chmod 0644 symlink
env TZ=utc touch -afm -t 197001020000.01 f1 symlink
xar -cf archive2.xar f1 symlink
od -t x1 archive2.xar | sed -E -e 's/^0[0-9]+//;s/^  //;s/(  )([0-9a-f]{2})/0x\2,/g;$ D' >  archive2.xar.txt
*/
static unsigned char archive2[] = {
0x78,0x61,0x72,0x21,0x00,0x1c,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xe8,
0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x84,0x00,0x00,0x00,0x01,0x78,0xda,0xb4,0x54,
0xcb,0x6e,0xa3,0x30,0x14,0xdd,0xf7,0x2b,0x90,0xf7,0x8c,0x1f,0x40,0x82,0x23,0xe3,
0xaa,0x9b,0x6a,0xf6,0xd3,0xd9,0xcc,0xce,0xf1,0x83,0x58,0xe5,0x25,0x70,0xaa,0xa4,
0x5f,0x3f,0xb6,0x09,0x4d,0xd3,0x30,0xdd,0x8d,0x84,0xc4,0xf5,0xf1,0xb9,0xc7,0x97,
0x7b,0x0f,0x66,0x8f,0xa7,0xb6,0x49,0xde,0xf4,0x38,0xd9,0xbe,0xab,0x00,0xfe,0x81,
0x40,0xa2,0x3b,0xd9,0x2b,0xdb,0xd5,0x15,0xf8,0xfd,0xf2,0x9c,0x96,0xe0,0x91,0x3f,
0xb0,0x93,0x18,0xf9,0x43,0xc2,0x5c,0x2f,0xfd,0x2b,0x61,0x72,0xd4,0xc2,0xf9,0x8c,
0xd4,0xd9,0x56,0x73,0x82,0x10,0x4d,0x31,0x49,0x11,0x79,0xc1,0xd9,0x2e,0x2b,0x76,
0xb8,0x60,0xf0,0x96,0x12,0x93,0x0e,0x5a,0xbe,0x4e,0xc7,0x36,0x99,0xdc,0xb9,0xd1,
0x15,0x98,0x0e,0x02,0x83,0xb0,0x93,0xb0,0xde,0x98,0x49,0x3b,0x8e,0x18,0xbc,0x44,
0x11,0x9d,0xec,0x7b,0x10,0x67,0x30,0x06,0x41,0x02,0x2e,0x1a,0x71,0x65,0x6c,0xa3,
0x13,0xab,0x2a,0x40,0x2e,0x32,0xf2,0xae,0x1c,0xb4,0xcb,0xd1,0x0e,0xd1,0x3f,0x3e,
0x73,0xa9,0x23,0x61,0xed,0x37,0xb4,0xf6,0x4a,0x13,0xdf,0xd0,0xc4,0x95,0x56,0x8f,
0xfd,0x71,0xe0,0xf2,0xa8,0x19,0x9c,0xc3,0x19,0xb5,0x8a,0x63,0x84,0xb0,0x07,0x7d,
0x14,0xa1,0xe3,0xa4,0xc7,0x99,0x17,0xa3,0x19,0xfb,0xa0,0x1d,0x17,0x5a,0xdb,0x2b,
0xcd,0xd1,0xb6,0xf0,0x3d,0x8c,0x61,0x04,0x1b,0xdb,0xbd,0x26,0xee,0x3c,0xf8,0xb6,
0x85,0xaf,0x06,0xdc,0xf8,0x94,0x00,0xce,0xdb,0x61,0x87,0x4f,0xe7,0x36,0x20,0x0c,
0xc6,0x55,0xc4,0x3b,0xd1,0x7e,0xc2,0xe3,0x2a,0xb6,0x31,0x68,0xdc,0xb6,0x70,0x99,
0x84,0x12,0x4e,0xc4,0xc8,0x9f,0xa9,0xbb,0xda,0x1d,0x38,0xc9,0xfc,0x49,0x73,0x38,
0xe3,0x97,0x11,0x91,0xdb,0x69,0x5d,0xc6,0x85,0x37,0xd7,0x71,0x79,0x6c,0xf1,0xd2,
0x32,0x73,0x31,0x0c,0x8d,0x95,0xd1,0x18,0xf0,0x94,0xd6,0xef,0x76,0x00,0xf0,0x42,
0x15,0xa3,0x3c,0xd8,0x37,0xad,0xd2,0xaf,0x3e,0xf9,0xf5,0xf3,0xc9,0x57,0x67,0xca,
0x2c,0x53,0xc4,0x28,0x89,0x69,0x96,0xd3,0x4c,0xec,0xf3,0x92,0x0a,0x25,0x48,0x66,
0x4a,0xb3,0xa5,0xd8,0x6c,0x88,0x91,0xd9,0x86,0x96,0x7e,0x36,0x5f,0x85,0x96,0x5a,
0x4e,0x6e,0x14,0xd2,0xfd,0xf3,0x84,0x42,0x6d,0xf3,0xbd,0xdc,0xcb,0x2d,0x55,0x99,
0xa1,0x7b,0x93,0x0b,0xe3,0xb5,0xf3,0xb2,0x2c,0x91,0xce,0x0b,0xb2,0x29,0x72,0xa3,
0x91,0xa4,0x94,0xc1,0x7b,0xa5,0xb9,0x79,0xf0,0xa3,0x7b,0x2b,0x56,0x9c,0xff,0x0c,
0xb2,0x66,0x45,0x4c,0xb7,0x28,0x45,0x38,0xd0,0x90,0x37,0x98,0x7f,0xf0,0x9a,0x15,
0xd7,0x69,0xff,0xdd,0x8a,0x9b,0x3c,0xff,0x6c,0xc5,0xe0,0xae,0x24,0x18,0xaa,0x02,
0xfd,0x68,0x6b,0xdb,0x89,0x06,0xf0,0x83,0x18,0xd5,0xaa,0xf9,0x82,0x4f,0xef,0x7c,
0xe7,0x59,0xe1,0x22,0x61,0x30,0x5e,0x2b,0x7f,0x01,0x00,0x00,0xff,0xff,0x03,0x00,
0x2b,0xab,0x4f,0xf9,0xbb,0xf7,0x90,0xb5,0x34,0x8f,0x7c,0xae,0x72,0xa0,0x80,0xd2,
0x69,0xc7,0xa2,0xe7,0x44,0x53,0xeb,0x75,0x78,0xda,0xca,0x48,0xcd,0xc9,0xc9,0xcf,
0x80,0x13,0x5c,0x00,0x00,0x00,0x00,0xff,0xff,0x03,0x00,0x37,0xf7,0x06,0x47
};

static void verify2(struct transform *a, struct transform_entry *ae)
{
	(void)a; /* UNUSED */
	assertEqualInt(archive_entry_filetype(ae), AE_IFLNK);
	assertEqualInt(archive_entry_mode(ae) & 0777, 0755);
	assertEqualInt(archive_entry_uid(ae), UID);
	assertEqualInt(archive_entry_gid(ae), GID);
	assertEqualString(archive_entry_uname(ae), UNAME);
	assertEqualString(archive_entry_gname(ae), GNAME);
	assertEqualString(archive_entry_pathname(ae), "symlink");
	assertEqualString(archive_entry_symlink(ae), "f1");
	assert(archive_entry_hardlink(ae) == NULL);
}

/* Character device node.
#How to make
mknod devchar c 0 30
chown $UNAME:$GNAME devchar
chmod 0644 devchar
env TZ=utc touch -afm -t 197001020000.01 devchar
xar -cf archive3.xar devchar
od -t x1 archive3.xar | sed -E -e 's/^0[0-9]+//;s/^  //;s/(  )([0-9a-f]{2})/0x\2,/g;$ D' >  archive3.xar.txt
*/
static unsigned char archive3[] = {
0x78,0x61,0x72,0x21,0x00,0x1c,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x38,
0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x3b,0x00,0x00,0x00,0x01,0x78,0xda,0x7c,0x92,
0x4d,0x6e,0xc3,0x20,0x10,0x85,0xf7,0x39,0x05,0xf2,0xde,0x05,0x9c,0x9f,0x36,0xd6,
0x84,0xec,0x7a,0x82,0x74,0xd3,0x1d,0xc2,0x93,0x98,0xd4,0x36,0x11,0xe0,0x28,0xe9,
0xe9,0x0b,0xe3,0xa4,0x69,0xa5,0xaa,0x92,0x25,0x1e,0x8f,0xef,0x8d,0x86,0xc1,0xb0,
0xbd,0xf4,0x1d,0x3b,0xa3,0x0f,0xd6,0x0d,0x9b,0x42,0x3e,0x89,0x82,0xe1,0x60,0x5c,
0x63,0x87,0xc3,0xa6,0x78,0xdb,0xbd,0x96,0x2f,0xc5,0x56,0xcd,0xe0,0xa2,0xbd,0x9a,
0x31,0x88,0xce,0xa4,0x85,0x81,0xf1,0xa8,0x63,0x4a,0x94,0xd1,0xf6,0xa8,0x2a,0x21,
0xd6,0xa5,0xac,0x4a,0x51,0xed,0xa4,0xa8,0xab,0x79,0x2d,0x57,0xc0,0x7f,0x23,0x14,
0x6a,0xd1,0x7c,0x84,0xb1,0x67,0x21,0x5e,0x3b,0xdc,0x14,0xa1,0xd5,0xb2,0xc8,0x27,
0x0c,0xdc,0x7e,0x1f,0x30,0x2a,0x01,0xfc,0xa6,0xc8,0x0d,0xf6,0x33,0x17,0x07,0x4e,
0x22,0x97,0xe0,0xf7,0x1a,0xb4,0xdb,0xdb,0x0e,0x99,0x6d,0x52,0xdb,0xb7,0x32,0xe6,
0xaf,0x76,0xaa,0x7a,0xb9,0x7c,0x4f,0xc9,0x7b,0x1f,0x0c,0x7a,0x92,0x72,0xfd,0x2c,
0x4a,0x21,0x33,0x26,0x44,0x9d,0x3f,0x99,0xb0,0xfe,0x81,0xe9,0x7f,0x30,0xfd,0xc0,
0x0e,0xde,0x8d,0x27,0x65,0x46,0x04,0x3e,0xc9,0xc9,0xb5,0x8d,0x92,0x42,0xc8,0x64,
0x26,0x45,0xd6,0x18,0xd0,0x4f,0x1c,0xa9,0xc9,0xfb,0xc6,0xc6,0x3b,0xd6,0xbb,0x06,
0x95,0x58,0x2d,0x16,0xa9,0x99,0x2c,0xc9,0x6c,0xf0,0x6c,0xcd,0xa4,0x13,0x61,0x07,
0xe7,0xd5,0x3c,0x0d,0x66,0x52,0x37,0x57,0x1f,0x93,0xce,0x26,0x09,0x8a,0xf1,0x1f,
0x39,0x88,0xd7,0x13,0x2a,0xd3,0x6a,0xaf,0x4d,0x44,0xcf,0xc2,0x09,0x8d,0xd5,0x1d,
0x70,0xf2,0x89,0x18,0x74,0xba,0x54,0x8a,0x64,0x08,0x38,0xed,0x68,0xea,0x79,0xd0,
0xf9,0xf9,0x39,0xbd,0x3f,0x70,0xfa,0x1b,0xbe,0x00,0x00,0x00,0xff,0xff,0x03,0x00,
0xab,0x43,0xa3,0xac,0x76,0x40,0x1e,0x8b,0x95,0x0d,0x28,0x79,0x79,0x43,0x49,0x4e,
0x16,0xa1,0x56,0x99,0x1f,0x83,0x77,0x41
};

static void verify3(struct transform *a, struct transform_entry *ae)
{
	(void)a; /* UNUSED */
	assertEqualInt(archive_entry_filetype(ae), AE_IFCHR);
	assertEqualInt(archive_entry_mode(ae) & 0777, 0644);
	assertEqualInt(archive_entry_uid(ae), UID);
	assertEqualInt(archive_entry_gid(ae), GID);
	assertEqualString(archive_entry_uname(ae), UNAME);
	assertEqualString(archive_entry_gname(ae), GNAME);
	assertEqualString(archive_entry_pathname(ae), "devchar");
	assert(archive_entry_symlink(ae) == NULL);
	assert(archive_entry_hardlink(ae) == NULL);
	assertEqualInt(archive_entry_mtime(ae), 86401);
}

/* Block device node.
#How to make
mknod devblock b 0 30
chown $UNAME:$GNAME devblock
chmod 0644 devblock
env TZ=utc touch -afm -t 197001020000.01 devblock
xar -cf archive4.xar devblock
od -t x1 archive4.xar | sed -E -e 's/^0[0-9]+//;s/^  //;s/(  )([0-9a-f]{2})/0x\2,/g;$ D' >  archive4.xar.txt
*/
static unsigned char archive4[] = {
0x78,0x61,0x72,0x21,0x00,0x1c,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x34,
0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x38,0x00,0x00,0x00,0x01,0x78,0xda,0x7c,0x92,
0xc1,0x6e,0xc2,0x30,0x0c,0x86,0xef,0x3c,0x45,0xd4,0x7b,0x17,0x07,0xd0,0x0a,0x95,
0x09,0xb7,0x3d,0x01,0xbb,0xec,0x96,0xa5,0x06,0x32,0xda,0xa6,0x6a,0x5a,0x04,0x7b,
0xfa,0x25,0x2e,0x8c,0x4d,0x9a,0x26,0x55,0xea,0x97,0x3f,0x9f,0x2d,0x37,0x29,0x6e,
0x2f,0x4d,0x2d,0xce,0xd4,0x07,0xe7,0xdb,0x4d,0xa6,0x9e,0x20,0x13,0xd4,0x5a,0x5f,
0xb9,0xf6,0xb0,0xc9,0x5e,0x77,0x2f,0xf9,0x2a,0xdb,0xea,0x19,0x5e,0x4c,0xaf,0x67,
0x02,0x07,0x6f,0xe3,0x4b,0xa0,0xed,0xc9,0x0c,0xb1,0x22,0x1f,0x5c,0x43,0x7a,0x0e,
0xb0,0xce,0xd5,0x3c,0x87,0xf9,0x4e,0x41,0xb9,0x58,0x95,0xaa,0x40,0xf9,0x5b,0xe1,
0xa2,0x23,0xd9,0x53,0x18,0x1b,0x11,0x86,0x6b,0x4d,0x9b,0x2c,0x1c,0x8d,0xca,0xd2,
0x8e,0x40,0xbf,0xdf,0x07,0x1a,0x34,0xa0,0xbc,0x11,0xa7,0xc1,0x7d,0xa6,0xe6,0x28,
0x19,0x52,0x0b,0x79,0xef,0xc1,0xab,0xbd,0xab,0x49,0xb8,0x2a,0x8e,0x7d,0x6b,0x63,
0xff,0x1e,0x07,0x8a,0xb7,0x58,0x79,0x9f,0x43,0x60,0xc3,0xa8,0xd6,0x05,0xe4,0xa0,
0x92,0x06,0x50,0xa6,0x47,0x45,0xad,0x79,0x68,0xe6,0x1f,0xcd,0x3c,0xb4,0x43,0xef,
0xc7,0x4e,0xdb,0x91,0x50,0x4e,0x38,0xa5,0xae,0xd2,0x0a,0x40,0xc5,0x30,0x12,0x47,
0x63,0xa0,0x7e,0xf2,0x98,0xa6,0xec,0x5b,0x1b,0xef,0x5a,0xe3,0x2b,0xd2,0xf0,0xbc,
0x5c,0xc6,0x61,0x12,0x72,0x58,0xd1,0xd9,0xd9,0x89,0xa3,0xe1,0x5a,0xdf,0xeb,0x45,
0x3c,0x98,0x89,0x6e,0xa9,0xf9,0x88,0x9c,0x42,0x06,0x2e,0x93,0x3f,0xea,0x70,0xb8,
0x76,0xa4,0xdf,0x6b,0x6f,0x4f,0x22,0x74,0x64,0x9d,0xa9,0x51,0x72,0xc6,0xbb,0xad,
0x89,0x1f,0x14,0x75,0x16,0x50,0xf2,0x92,0x8f,0x3c,0x9d,0x72,0xba,0x7b,0xc9,0x97,
0x8f,0x92,0x7f,0x85,0x2f,0x00,0x00,0x00,0xff,0xff,0x03,0x00,0xbe,0x66,0xa2,0x82,
0x3a,0x54,0xd3,0x61,0xaa,0x8e,0x30,0x4c,0xc8,0x36,0x3b,0x7a,0xa4,0xb9,0xef,0xfc,
0x7a,0x5d,0x21,0xde
};

static void verify4(struct transform *a, struct transform_entry *ae)
{
	(void)a; /* UNUSED */
	assertEqualInt(archive_entry_filetype(ae), AE_IFBLK);
	assertEqualInt(archive_entry_mode(ae) & 0777, 0644);
	assertEqualInt(archive_entry_uid(ae), UID);
	assertEqualInt(archive_entry_gid(ae), GID);
	assertEqualString(archive_entry_uname(ae), UNAME);
	assertEqualString(archive_entry_gname(ae), GNAME);
	assertEqualString(archive_entry_pathname(ae), "devblock");
	assert(archive_entry_symlink(ae) == NULL);
	assert(archive_entry_hardlink(ae) == NULL);
	assertEqualInt(archive_entry_mtime(ae), 86401);
}

/* Directory.
#How to make
mkdir dir1
chown $UNAME:$GNAME dir1
chmod 0755 dir1
env TZ=utc touch -afm -t 197001020000.01 dir1
xar -cf archive5.xar dir1
od -t x1 archive5.xar | sed -E -e 's/^0[0-9]+//;s/^  //;s/(  )([0-9a-f]{2})/0x\2,/g;$ D' >  archive5.xar.txt
*/
static unsigned char archive5[] = {
0x78,0x61,0x72,0x21,0x00,0x1c,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x16,
0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xec,0x00,0x00,0x00,0x01,0x78,0xda,0x74,0x91,
0xc1,0x6e,0xc2,0x30,0x0c,0x86,0xef,0x3c,0x45,0xd4,0x7b,0x17,0xa7,0x83,0x31,0xaa,
0x34,0xdc,0xf6,0x04,0xec,0xb2,0x5b,0x95,0x1a,0x88,0x68,0x1a,0x94,0xa4,0x13,0xdd,
0xd3,0x2f,0x71,0xe9,0xd0,0xa4,0x4d,0xaa,0xd4,0x3f,0xbf,0x3f,0xff,0xb2,0x6c,0xb9,
0xbf,0xd9,0x9e,0x7d,0xa2,0x0f,0xc6,0x0d,0x4d,0x21,0x9e,0xa0,0x60,0x38,0x68,0xd7,
0x99,0xe1,0xd4,0x14,0xef,0x87,0xb7,0xf2,0xb5,0xd8,0xab,0x95,0xbc,0xb5,0x5e,0xad,
0x98,0x8c,0x4e,0xa7,0x1f,0x93,0xda,0x63,0x1b,0x53,0x47,0x19,0x8d,0x45,0x55,0x01,
0xec,0x4a,0x51,0x95,0x50,0x1d,0x04,0xd4,0x6b,0x51,0xaf,0x37,0x92,0xff,0x46,0xa8,
0xe9,0x8c,0xfa,0x12,0x46,0xcb,0x42,0x9c,0x7a,0x6c,0x8a,0x70,0x6e,0x45,0x91,0x2b,
0x4c,0xba,0xe3,0x31,0x60,0x54,0x20,0xf9,0x5d,0x91,0x1b,0xcc,0x57,0x0e,0x97,0x9c,
0x44,0x8e,0xe0,0x4b,0x06,0xbd,0x8e,0xa6,0x47,0x66,0xba,0x34,0xf6,0x3d,0x46,0xff,
0x3d,0xce,0x33,0x7c,0xa4,0xce,0x65,0x0e,0x26,0x2d,0x49,0xb1,0xdb,0x42,0x09,0x22,
0x63,0x00,0x75,0xfe,0x44,0xc2,0xec,0x03,0x6b,0xff,0x49,0x7b,0x49,0x58,0xfb,0xc0,
0x4e,0xde,0x8d,0x57,0xa5,0x47,0x94,0x7c,0x96,0xb3,0x6b,0x3a,0x25,0x00,0x44,0x32,
0x93,0x22,0x6b,0x0c,0xe8,0x67,0x8e,0xd4,0xec,0xfd,0x60,0xe3,0x82,0x59,0xd7,0xa1,
0x82,0xed,0x26,0xed,0x90,0x24,0x99,0x71,0xba,0xa2,0xea,0x8c,0x47,0x1d,0x9d,0x9f,
0x24,0xa7,0x37,0x55,0x86,0xd6,0x52,0x25,0x45,0x90,0xa4,0x35,0xe5,0xcd,0xe4,0x7b,
0x71,0x3a,0x98,0xe4,0x74,0xbe,0x6f,0x00,0x00,0x00,0xff,0xff,0x03,0x00,0x23,0x7a,
0x8c,0x2f,0x78,0xe9,0x69,0x28,0x93,0x14,0x72,0x68,0x8d,0xeb,0x42,0x7b,0xf6,0x0f,
0x70,0x64,0xa3,0xff,0xb9,0x35
};

static void verify5(struct transform *a, struct transform_entry *ae)
{
	(void)a; /* UNUSED */
	assertEqualInt(archive_entry_filetype(ae), AE_IFDIR);
	assertEqualInt(archive_entry_mtime(ae), 86401);
	assertEqualInt(archive_entry_mode(ae) & 0777, 0755);
	assertEqualInt(archive_entry_uid(ae), UID);
	assertEqualInt(archive_entry_gid(ae), GID);
	assertEqualString(archive_entry_uname(ae), UNAME);
	assertEqualString(archive_entry_gname(ae), GNAME);
}

/* fifo
#How to make
mkfifo -m 0755 fifo
chown $UNAME:$GNAME fifo
env TZ=utc touch -afm -t 197001020000.01 fifo
xar -cf archive6.xar fifo
od -t x1 archive6.xar | sed -E -e 's/^0[0-9]+//;s/^  //;s/(  )([0-9a-f]{2})/0x\2,/g;$ D' >  archive6.xar.txt
*/
static unsigned char archive6[] = {
0x78,0x61,0x72,0x21,0x00,0x1c,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x0e,
0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xe7,0x00,0x00,0x00,0x01,0x78,0xda,0x7c,0x91,
0xc1,0x6e,0xc3,0x20,0x0c,0x86,0xef,0x7d,0x0a,0xc4,0x3d,0xc3,0x64,0xab,0xda,0x46,
0x94,0xde,0xf6,0x04,0xdd,0x65,0x37,0x44,0x9c,0x16,0x2d,0x84,0x2a,0x90,0xa9,0xdd,
0xd3,0x0f,0x9c,0x66,0xd5,0xa4,0x69,0x12,0x52,0xbe,0xfc,0x7c,0xb6,0x2c,0xac,0x0e,
0x57,0xdf,0xb3,0x4f,0x1c,0xa3,0x0b,0xc3,0x9e,0xcb,0x27,0xe0,0x0c,0x07,0x1b,0x5a,
0x37,0x9c,0xf6,0xfc,0xed,0xf8,0x5a,0x6d,0xf9,0x41,0xaf,0xd4,0xd5,0x8c,0x7a,0xc5,
0x54,0x0a,0x36,0x7f,0x98,0xb2,0x23,0x9a,0x94,0x2b,0xaa,0xe4,0x3c,0xea,0x1a,0x60,
0x57,0xc9,0xba,0x82,0xfa,0x28,0x65,0xf3,0x02,0x4d,0xbd,0x55,0xe2,0xb7,0x42,0x45,
0x67,0xb4,0x1f,0x71,0xf2,0x2c,0xa6,0x5b,0x8f,0x7b,0x1e,0xcf,0x46,0xf2,0x72,0xc3,
0x54,0xe8,0xba,0x88,0x49,0x83,0x12,0x77,0xa2,0x34,0xba,0xaf,0xd2,0x5c,0x09,0x82,
0xd2,0x42,0x2c,0x3d,0xe8,0xaf,0x73,0x3d,0x32,0xd7,0xe6,0xb1,0xef,0x6d,0xec,0xdf,
0xe3,0xc8,0xe7,0xf7,0x5c,0xb9,0xcc,0xc1,0x94,0x27,0x94,0xbb,0x0d,0x54,0x20,0x8b,
0x06,0xd0,0x94,0x23,0xb3,0xe6,0x1f,0x9a,0xf9,0x47,0x33,0x0f,0xed,0x34,0x86,0xe9,
0xa2,0xed,0x84,0x4a,0xcc,0x38,0xa7,0xae,0xd5,0x12,0x40,0xe6,0x30,0x13,0x45,0x53,
0xc4,0x71,0xf6,0x88,0xe6,0xec,0x47,0x9b,0x16,0xcd,0x87,0x16,0x35,0x6c,0xd6,0xeb,
0x3c,0x4c,0x41,0x0a,0xd3,0xed,0x82,0xba,0x73,0x5d,0x50,0x82,0x90,0xc2,0xc1,0xf8,
0x25,0x24,0xa4,0x17,0x2a,0x8f,0x52,0x56,0x25,0x68,0x57,0x4a,0xd0,0xe6,0xbe,0x01,
0x00,0x00,0xff,0xff,0x03,0x00,0x44,0x19,0x8a,0x2a,0x82,0xbc,0x8c,0xae,0x97,0xa7,
0x7d,0x65,0xa5,0x82,0xdb,0xaa,0xc2,0xcb,0xbe,0xf0,0x1f,0xd1,0xf9,0x56
};

static void verify6(struct transform *a, struct transform_entry *ae)
{
	(void)a; /* UNUSED */
	assertEqualInt(archive_entry_filetype(ae), AE_IFIFO);
	assertEqualInt(archive_entry_mode(ae) & 0777, 0755);
	assertEqualInt(archive_entry_uid(ae), UID);
	assertEqualInt(archive_entry_gid(ae), GID);
	assertEqualString(archive_entry_uname(ae), UNAME);
	assertEqualString(archive_entry_gname(ae), GNAME);
	assertEqualString(archive_entry_pathname(ae), "fifo");
	assert(archive_entry_symlink(ae) == NULL);
	assert(archive_entry_hardlink(ae) == NULL);
	assertEqualInt(archive_entry_mtime(ae), 86401);
}

/* Verify that a file records with directory name.
#How to make
mkdir dir1
echo "hellohellohello" > dir1/f1
chown $UNAME:$GNAME dir1/f1
chmod 0644 dir1/f1
env TZ=utc touch -afm -t 197001020000.01 dir1/f1
xar -cf archive7.xar dir1/f1
od -t x1 archive7.xar | sed -E -e 's/^0[0-9]+//;s/^  //;s/(  )([0-9a-f]{2})/0x\2,/g;$ D' >  archive7.xar.txt
*/

static unsigned char archive7[] = {
0x78,0x61,0x72,0x21,0x00,0x1c,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xbb,
0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x8a,0x00,0x00,0x00,0x01,0x78,0xda,0x7c,0x53,
0xc9,0x6e,0xdb,0x30,0x14,0xbc,0xe7,0x2b,0x04,0xdd,0x55,0x2e,0xa2,0x16,0x1a,0xb4,
0x82,0x5e,0x8a,0xdc,0x93,0x5e,0x7a,0xa3,0xb9,0xd8,0x44,0xb5,0x41,0xa2,0x02,0x3b,
0x5f,0x5f,0x92,0xa2,0x1c,0xbb,0x59,0x00,0x01,0x1a,0x3e,0xce,0x1b,0x0d,0x9f,0x86,
0xec,0xf1,0xdc,0xb5,0xc9,0xab,0x9a,0x66,0x33,0xf4,0xfb,0x14,0xfd,0x80,0x69,0xa2,
0x7a,0x31,0x48,0xd3,0x1f,0xf7,0xe9,0xef,0x97,0x5f,0x59,0x9d,0x3e,0x36,0x0f,0xec,
0xcc,0xa7,0xe6,0x21,0x61,0x76,0x10,0xee,0x95,0x30,0x31,0x29,0x6e,0x5d,0x47,0x66,
0x4d,0xa7,0x1a,0x0c,0x21,0xcd,0x10,0xce,0x20,0x7e,0x41,0x68,0x57,0xe0,0x5d,0x51,
0x31,0x70,0x4f,0x09,0x4d,0x27,0x25,0xfe,0xce,0x4b,0x97,0xcc,0xf6,0xd2,0xaa,0x7d,
0x3a,0x9f,0x38,0x4a,0xfd,0x4e,0xc2,0x06,0xad,0x67,0x65,0x1b,0xc8,0x40,0x44,0xa1,
0x3a,0x9b,0x37,0x2f,0xce,0x40,0x00,0x5e,0x02,0x6c,0x1a,0x61,0xa5,0x4d,0xab,0x12,
0x23,0x9d,0xed,0x28,0x63,0x2f,0xa3,0x6a,0xa4,0x99,0x94,0xb0,0xc3,0x74,0x61,0x20,
0xac,0xc3,0x4e,0xcf,0xbb,0xb0,0x83,0x18,0x08,0x30,0x14,0xaf,0xfd,0x78,0xed,0x4f,
0x98,0xe4,0x96,0xaf,0x30,0x61,0xad,0xea,0x8f,0xf6,0xd4,0xe0,0x9c,0x81,0x08,0xe3,
0x46,0xb4,0x88,0xef,0xdd,0x6e,0x7e,0x51,0xf9,0xee,0xd7,0x17,0xb7,0x69,0x6e,0xa7,
0xe6,0xe3,0xd8,0x1a,0x11,0x46,0x03,0xce,0xd9,0xf1,0xcd,0x8c,0x29,0xd8,0xb8,0x7c,
0x12,0x27,0xf3,0xaa,0x64,0xf6,0xff,0xa8,0x9e,0x9f,0x7e,0xba,0x33,0xea,0x3a,0xcf,
0x25,0xd6,0x52,0x20,0x9a,0x13,0x9a,0xf3,0x03,0xa9,0x29,0x97,0x1c,0xe7,0xba,0xd6,
0x15,0x45,0xba,0xc4,0x5a,0xe4,0x25,0xad,0x19,0xf8,0x20,0x74,0x75,0x73,0xb6,0x13,
0x17,0xf6,0xcb,0x4f,0x14,0xb2,0x22,0x07,0x71,0x10,0x15,0x95,0xb9,0xa6,0x07,0x4d,
0xb8,0x76,0xe2,0xa4,0xae,0x6b,0xa8,0x48,0x81,0xcb,0x82,0x68,0x05,0x05,0xa5,0x0c,
0x7c,0x54,0x8a,0x33,0x04,0xef,0x43,0x64,0xe2,0xf3,0x7c,0x90,0xfa,0x8f,0xfb,0x95,
0x5b,0x30,0x1c,0xaf,0x0b,0x18,0xd1,0x0a,0x66,0x10,0x79,0x1e,0x84,0x3b,0xff,0x20,
0xc7,0xeb,0x6e,0x78,0xfc,0x1b,0x1e,0xbf,0xe1,0x1d,0xa7,0x61,0x19,0x1b,0xb1,0x28,
0x06,0x56,0x18,0xcb,0x46,0x36,0x08,0x42,0x17,0x02,0x8f,0xd6,0xda,0x32,0xab,0x69,
0x65,0x06,0x14,0x8b,0x57,0xe2,0x72,0x25,0x76,0x83,0x54,0x0d,0x2c,0x09,0x71,0x96,
0x3c,0x5c,0xab,0x21,0x62,0x3e,0x48,0x37,0x69,0x8b,0x71,0xd3,0x77,0x61,0x03,0x9e,
0xb4,0x86,0x38,0x22,0xd7,0xe1,0xaf,0x13,0x03,0xe1,0x72,0xfd,0x03,0x00,0x00,0xff,
0xff,0x03,0x00,0x8d,0xb1,0x06,0x76,0xa6,0x7a,0xc3,0xbb,0x13,0x3d,0x45,0xe2,0x2b,
0x3b,0xd0,0x88,0xc7,0x58,0x7b,0xbd,0x30,0x9d,0x01,0x44,0x78,0xda,0xca,0x48,0xcd,
0xc9,0xc9,0xcf,0x80,0x13,0x5c,0x00,0x00,0x00,0x00,0xff,0xff,0x03,0x00,0x37,0xf7,
0x06,0x47
};

static void verify7(struct transform *a, struct transform_entry *ae)
{
	(void)a; /* UNUSED */
	assert(archive_entry_filetype(ae) == AE_IFREG);
	assertEqualInt(archive_entry_mode(ae) & 0777, 0644);
	assertEqualInt(archive_entry_uid(ae), UID);
	assertEqualInt(archive_entry_gid(ae), GID);
	assertEqualString(archive_entry_uname(ae), UNAME);
	assertEqualString(archive_entry_gname(ae), GNAME);
	assertEqualString(archive_entry_pathname(ae), "dir1/f1");
	assert(archive_entry_hardlink(ae) == NULL);
	assert(archive_entry_symlink(ae) == NULL);
	assertEqualInt(archive_entry_mtime(ae), 86401);
}

/* Verify that a file records with bzip2 compression
#How to make
echo "hellohellohello" > f1
chown $UNAME:$GNAME f1
chmod 0644 f1
env TZ=utc touch -afm -t 197001020000.01 f1
xar --compression bzip2 -cf archive8.xar f1
od -t x1 archive8.xar | sed -E -e 's/^0[0-9]+//;s/^  //;s/(  )([0-9a-f]{2})/0x\2,/g;$ D' >  archive8.xar.txt
*/

static unsigned char archive8[] = {
0x78,0x61,0x72,0x21,0x00,0x1c,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xb1,
0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x42,0x00,0x00,0x00,0x01,0x78,0xda,0x7c,0x53,
0xcb,0x6e,0xdc,0x20,0x14,0xdd,0xe7,0x2b,0x90,0xf7,0x0e,0x60,0xe3,0x07,0x23,0x86,
0xa8,0x9b,0xa8,0xfb,0x4e,0x37,0xdd,0x61,0x1e,0x63,0x14,0xbf,0x64,0xe3,0x68,0x92,
0xaf,0x2f,0x60,0x3b,0xa3,0x34,0x6d,0x25,0x4b,0x3e,0x1c,0x0e,0xe7,0x5e,0xee,0xe5,
0xb2,0xa7,0x5b,0xdf,0x81,0x57,0x3d,0x2f,0x76,0x1c,0xce,0x09,0x7e,0x44,0x09,0xd0,
0x83,0x1c,0x95,0x1d,0xae,0xe7,0xe4,0xe7,0xe5,0x39,0xad,0x93,0x27,0xfe,0xc0,0x6e,
0x62,0xe6,0x0f,0x80,0xb9,0x51,0xfa,0x1f,0x60,0x72,0xd6,0xc2,0xf9,0x13,0xa9,0xb3,
0xbd,0xe6,0x19,0x42,0x34,0xc5,0x59,0x8a,0xc8,0x05,0xd1,0x13,0xc6,0x27,0x9c,0x33,
0xf8,0x59,0x12,0x0f,0xb5,0x5a,0xbe,0x2c,0x6b,0x0f,0x16,0xf7,0xd6,0xe9,0x73,0xb2,
0xb4,0x02,0x27,0x61,0x07,0xb0,0xd1,0x98,0x45,0x3b,0x8e,0x18,0xdc,0x51,0x64,0x17,
0xfb,0x1e,0xcc,0x19,0x8c,0x20,0x58,0xc0,0xc3,0x23,0xae,0x8c,0xed,0x34,0xb0,0xca,
0xa7,0xbd,0xdb,0x28,0xe1,0x44,0x44,0x80,0x75,0x7a,0xb8,0xba,0x96,0x13,0xc2,0xe0,
0x0e,0x37,0x7e,0xf7,0xcf,0x3e,0x87,0xda,0x63,0xe1,0xf2,0x1e,0xcb,0x73,0x47,0x21,
0x8e,0x84,0xc5,0x34,0x75,0x56,0xc6,0x5b,0xc1,0x5b,0xda,0xbc,0xdb,0x29,0x4b,0xe0,
0xae,0x15,0xb3,0x6c,0xed,0xab,0x56,0xe9,0x9f,0xb7,0xfc,0xf1,0xfd,0x9b,0x4f,0xcf,
0xe4,0xa4,0x28,0x4a,0x94,0xcb,0x3a,0xcf,0x9b,0x26,0x93,0xaa,0x92,0xba,0x29,0xa8,
0x2a,0x89,0x29,0xa8,0x50,0x22,0x97,0x45,0xa1,0x71,0xe5,0xeb,0xf6,0xc5,0xe8,0x48,
0xe6,0xe6,0x66,0x21,0xdd,0x3f,0x23,0x14,0xaa,0x22,0x8d,0x6c,0x64,0x45,0x55,0x6e,
0x68,0x63,0x88,0x30,0xa6,0x36,0xa4,0xae,0x6b,0xa4,0x49,0x91,0x95,0x05,0x31,0x1a,
0x49,0x4a,0x19,0xfc,0xea,0xb4,0x55,0x0f,0x7e,0x94,0x8f,0xc9,0xbf,0xf7,0x15,0xd5,
0xbf,0x7c,0x0b,0x8e,0x86,0x02,0xd6,0x47,0x88,0x69,0x85,0x52,0x84,0x53,0x94,0x5d,
0x10,0x3a,0x85,0x0f,0x7b,0x59,0x7f,0x97,0x89,0xff,0xc8,0xc4,0x5d,0x76,0x9d,0xc7,
0x75,0xe2,0x72,0xd5,0x0c,0x6e,0x70,0x63,0xad,0xe2,0x18,0x21,0xec,0x49,0x8f,0x22,
0xb5,0x2e,0x7a,0xde,0x74,0x11,0x6d,0xdc,0x87,0x6c,0x3d,0x64,0xfd,0xa8,0x34,0x47,
0x65,0x78,0x02,0x11,0x46,0xd2,0xbd,0x4d,0x1a,0x74,0x76,0x78,0x39,0x27,0xe3,0x6c,
0xaf,0x76,0x10,0x5d,0xc2,0x5b,0x31,0xab,0xc0,0x31,0x18,0xb6,0x37,0xe1,0x20,0x7c,
0x5e,0xc6,0xfb,0x45,0x10,0x1f,0x5f,0x78,0x6f,0x61,0x0a,0x60,0x1c,0x03,0x06,0xe3,
0x50,0xfc,0x06,0x00,0x00,0xff,0xff,0x03,0x00,0x19,0xcf,0xf5,0xc0,0xf9,0x65,0xe8,
0x78,0xc3,0xfa,0x5f,0x0a,0xf6,0x09,0x17,0xd8,0xb0,0x54,0xb9,0x02,0x8d,0x91,0x31,
0x9c,0x42,0x5a,0x68,0x39,0x31,0x41,0x59,0x26,0x53,0x59,0xc1,0x52,0x36,0xf7,0x00,
0x00,0x03,0x41,0x00,0x00,0x10,0x02,0x44,0xa0,0x00,0x21,0xb4,0x01,0x9a,0x0d,0x46,
0xa5,0x32,0x38,0xbb,0x92,0x29,0xc2,0x84,0x86,0x0a,0x91,0xb7,0xb8
};

/* Verify that a file records with no compression
#How to make
echo "hellohellohello" > f1
chown $UNAME:$GNAME f1
chmod 0644 f1
env TZ=utc touch -afm -t 197001020000.01 f1
xar --compression none -cf archive9.xar f1
od -t x1 archive9.xar | sed -E -e 's/^0[0-9]+//;s/^  //;s/(  )([0-9a-f]{2})/0x\2,/g;$ D' >  archive9.xar.txt
*/

static unsigned char archive9[] = {
0x78,0x61,0x72,0x21,0x00,0x1c,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x98,
0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x47,0x00,0x00,0x00,0x01,0x78,0xda,0xa4,0x53,
0x4d,0x6f,0xe3,0x20,0x14,0xbc,0xf7,0x57,0x20,0xee,0x5e,0xc0,0x25,0x89,0x1d,0x11,
0xaa,0x5e,0xaa,0xbd,0x6f,0xf6,0xb2,0x37,0x02,0x8f,0x18,0xc5,0x1f,0x11,0xc6,0x55,
0xba,0xbf,0x7e,0x01,0xdb,0xad,0xba,0x55,0x7b,0xa9,0x64,0xc9,0xe3,0x61,0xde,0x78,
0x78,0xf0,0xc4,0xc3,0xad,0x6b,0xd1,0x33,0xf8,0xd1,0x0d,0xfd,0x01,0xb3,0x1f,0x14,
0x23,0xe8,0xf5,0x60,0x5c,0x7f,0x3e,0xe0,0xdf,0xc7,0xa7,0xa2,0xc2,0x0f,0xf2,0x4e,
0xdc,0x94,0x97,0x77,0x48,0x84,0x41,0xc7,0x17,0x12,0xda,0x83,0x0a,0xb1,0xa2,0x08,
0xae,0x03,0x59,0x52,0x5a,0x17,0xac,0x2c,0x28,0x3f,0xd2,0x7a,0xcf,0xaa,0x3d,0xaf,
0x05,0x79,0x2f,0xc9,0x45,0x0d,0xe8,0xcb,0x38,0x75,0x68,0x0c,0x2f,0x2d,0x1c,0xf0,
0xd8,0x28,0x86,0xd3,0x0a,0x12,0x83,0xb5,0x23,0x04,0x49,0x05,0x59,0x50,0x66,0x47,
0xf7,0x37,0x99,0x0b,0x92,0x41,0xb2,0x20,0xab,0x47,0xfe,0xb2,0xae,0x05,0xe4,0x4c,
0x8c,0xbd,0xd8,0x18,0x15,0x54,0x46,0x48,0xb4,0xd0,0x9f,0x43,0x23,0xd9,0x56,0x90,
0x05,0xce,0xfc,0xba,0xb9,0x35,0x84,0xba,0x5e,0x5b,0xa7,0x73,0x52,0x32,0xe8,0x00,
0xa1,0x18,0x43,0x4c,0xde,0x61,0xb2,0x14,0x2c,0x81,0xca,0xf7,0xd9,0x96,0x70,0xc9,
0x7e,0x0d,0x17,0x39,0xe5,0x75,0xe3,0x9e,0xc1,0x14,0xff,0x6f,0xf5,0xd7,0xcf,0xc7,
0x98,0x71,0x63,0x76,0xfc,0xa4,0x4f,0x7a,0x57,0x9b,0x7b,0x5b,0x9f,0x2c,0x57,0xd6,
0x56,0x96,0x57,0x55,0x45,0x81,0x6f,0xca,0xed,0x86,0x5b,0xa0,0xba,0x8e,0xcd,0xfb,
0x60,0xb4,0xa6,0xbf,0x05,0xaf,0x62,0xca,0xef,0xff,0xe1,0xa3,0xd3,0xdc,0x42,0xf2,
0xda,0x43,0xa1,0x3f,0x39,0xdc,0xed,0x9f,0x78,0x0e,0xeb,0xa9,0x22,0xd1,0x65,0xc8,
0xea,0x1d,0x2d,0x28,0x2b,0x68,0x79,0xa4,0x74,0x9f,0x1e,0x16,0x65,0xdd,0x9b,0x4c,
0x7d,0x21,0x53,0x6f,0xb2,0xb3,0x1f,0xa6,0xab,0xd4,0x13,0x08,0x32,0xc3,0x99,0x75,
0x46,0x32,0x4a,0x59,0x24,0x23,0xca,0xd4,0x34,0x82,0x9f,0x75,0x19,0xcd,0xdc,0xab,
0x6c,0x5a,0x65,0xdd,0x60,0x40,0xd2,0x2d,0xe7,0x31,0x4c,0x82,0x99,0x0c,0x2f,0x57,
0x40,0xad,0xeb,0x2f,0x07,0x3c,0x78,0x77,0x76,0xbd,0x6a,0xb1,0x6c,0x94,0x37,0x89,
0x13,0x24,0x2d,0xcf,0xc2,0x5e,0xc5,0x5c,0x36,0xfa,0x65,0x90,0x6f,0x60,0xba,0x74,
0x69,0x14,0x48,0x9e,0x05,0x41,0xf2,0x64,0xfc,0x03,0x00,0x00,0xff,0xff,0x03,0x00,
0xee,0x8e,0xf8,0x75,0xa1,0xaf,0x74,0x71,0x3f,0x40,0x08,0xab,0x13,0x7d,0xc0,0x82,
0x3a,0x56,0xeb,0x4e,0x35,0xf1,0x35,0xb7,0x68,0x65,0x6c,0x6c,0x6f,0x68,0x65,0x6c,
0x6c,0x6f,0x68,0x65,0x6c,0x6c,0x6f,0x0a
};

/* Verify that a file records with md5 hashing algorithm
#How to make
echo "hellohellohello" > f1
chown $UNAME:$GNAME f1
chmod 0644 f1
env TZ=utc touch -afm -t 197001020000.01 f1
xar --toc-cksum md5 -cf archive10.xar f1
od -t x1 archive10.xar | sed -E -e 's/^0[0-9]+//;s/^  //;s/(  )([0-9a-f]{2})/0x\2,/g;$ D' >  archive10.xar.txt
*/

static unsigned char archive10[] = {
0x78,0x61,0x72,0x21,0x00,0x1c,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xaf,
0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x40,0x00,0x00,0x00,0x02,0x78,0xda,0x7c,0x53,
0x4d,0x6f,0xdc,0x20,0x10,0xbd,0xe7,0x57,0x20,0xee,0x0e,0x60,0xb3,0xb6,0x59,0xb1,
0x44,0xbd,0x44,0xbd,0x77,0x7b,0xe9,0x8d,0xe5,0xc3,0x8b,0xe2,0x2f,0x61,0x1c,0x6d,
0xf2,0xeb,0x8b,0xb1,0x9d,0xb4,0x4d,0x52,0xc9,0x92,0x1f,0x8f,0xc7,0x9b,0x61,0x86,
0xe1,0x0f,0xb7,0xae,0x05,0xcf,0xc6,0x4f,0x6e,0xe8,0x4f,0x90,0xdc,0x63,0x08,0x4c,
0xaf,0x06,0xed,0xfa,0xe6,0x04,0x7f,0x9e,0x1f,0xb3,0x1a,0x3e,0x88,0x3b,0x7e,0x93,
0x5e,0xdc,0x01,0x1e,0x06,0x15,0x7f,0x80,0x2b,0x6f,0x64,0x88,0x27,0xb2,0xe0,0x3a,
0x23,0x72,0x8c,0x59,0x46,0xf2,0x0c,0xd3,0x33,0x66,0xc7,0x02,0x1f,0x69,0xcd,0xd1,
0xdf,0x92,0x74,0xe8,0x6a,0xd4,0xd3,0x34,0x77,0x60,0x0a,0x2f,0xad,0x39,0xc1,0x4e,
0x1f,0xe0,0xb2,0x01,0xf8,0x60,0xed,0x64,0x82,0xc0,0x1c,0x6d,0x28,0xb1,0x93,0x7b,
0x35,0x82,0x94,0x1c,0x25,0xb0,0x38,0xa0,0xdd,0x22,0xad,0xac,0x6b,0x0d,0x70,0x3a,
0x66,0xbd,0xd9,0x68,0x19,0x64,0x42,0x80,0xb7,0xa6,0x6f,0xc2,0x55,0xe4,0x05,0x47,
0x1b,0x5c,0xf9,0xcd,0x7f,0x71,0xfd,0x23,0xd4,0x27,0xb1,0x22,0xb7,0xd7,0x61,0xcf,
0x57,0x8e,0x63,0xeb,0x54,0xba,0x14,0xba,0x65,0xcd,0xab,0x1b,0x21,0xda,0xa4,0xd2,
0xab,0xab,0x7b,0x36,0x3a,0xfb,0xf7,0x8e,0x3f,0xbe,0x7f,0x8b,0xd9,0xd9,0xba,0x28,
0x74,0x6e,0xb5,0x22,0xac,0xa0,0xac,0x90,0x17,0x5a,0x33,0xa9,0x65,0x5e,0xd8,0xda,
0x56,0x8c,0xd8,0x32,0xb7,0xaa,0x28,0x59,0xac,0xda,0x07,0xa3,0x3d,0x97,0x5b,0xf0,
0x52,0x85,0x2f,0x23,0x1c,0x74,0x45,0x2f,0xea,0xa2,0x2a,0xa6,0x0b,0xcb,0x2e,0x96,
0x4a,0x1b,0xbd,0x69,0x5d,0xd7,0xd8,0xd0,0x43,0x5e,0x1e,0xa8,0x35,0x58,0x31,0xc6,
0xd1,0x47,0xa7,0xb5,0x78,0xe8,0xad,0x7a,0x5c,0x7d,0xd1,0xd5,0xea,0x57,0xec,0xc0,
0xde,0x4e,0xc0,0xbb,0x04,0x09,0xab,0x70,0x86,0x49,0x86,0xf3,0x33,0xc6,0xc7,0xe5,
0x23,0x51,0xd6,0xbd,0xcb,0xe4,0x7f,0x64,0xf2,0x5d,0xd6,0xf8,0x61,0x1e,0x85,0x9a,
0x0d,0x47,0x2b,0x5c,0x59,0xa7,0x05,0xc1,0x98,0x44,0x32,0xa2,0x44,0xcd,0x93,0xf1,
0xab,0x2e,0xa1,0x95,0x7b,0x93,0xcd,0xbb,0xac,0x1b,0xb4,0x11,0xb8,0xa4,0x34,0x26,
0xb3,0xc0,0x44,0x86,0x97,0xd1,0x80,0xd6,0xf5,0x4f,0x27,0x38,0x78,0xd7,0xb8,0x5e,
0xb6,0x50,0x5c,0xa5,0xd7,0x0b,0xc7,0xd1,0xb2,0xbd,0x0a,0x7b,0x19,0xf3,0xb2,0xd1,
0x2f,0x81,0xf4,0xf6,0x96,0xe7,0xb6,0xcc,0x00,0x4a,0x43,0xc0,0x51,0x1a,0x89,0xdf,
0x00,0x00,0x00,0xff,0xff,0x03,0x00,0x27,0xf8,0xf5,0x28,0x87,0x01,0xb1,0xb7,0x18,
0xe8,0x34,0x20,0x06,0x5c,0x66,0x9a,0x43,0x26,0xe7,0x94,0x78,0xda,0xca,0x48,0xcd,
0xc9,0xc9,0xcf,0x80,0x13,0x5c,0x00,0x00,0x00,0x00,0xff,0xff,0x03,0x00,0x37,0xf7,
0x06,0x47
};

/* Verify that a file records with no hashing algorithm
#How to make
echo "hellohellohello" > f1
chown $UNAME:$GNAME f1
chmod 0644 f1
env TZ=utc touch -afm -t 197001020000.01 f1
xar --toc-cksum none -cf archive11.xar f1
od -t x1 archive11.xar | sed -E -e 's/^0[0-9]+//;s/^  //;s/(  )([0-9a-f]{2})/0x\2,/g;$ D' >  archive11.xar.txt
*/

static unsigned char archive11[] = {
0x78,0x61,0x72,0x21,0x00,0x1c,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x98,
0x00,0x00,0x00,0x00,0x00,0x00,0x02,0xef,0x00,0x00,0x00,0x00,0x78,0xda,0x7c,0x52,
0xcb,0x6e,0xeb,0x20,0x14,0xdc,0xf7,0x2b,0x10,0x7b,0x17,0xb0,0x89,0x63,0x22,0x42,
0x75,0x37,0x55,0xf7,0xcd,0xdd,0x74,0x47,0x78,0x38,0xa8,0x7e,0xc9,0xc6,0x55,0xda,
0xaf,0xbf,0x3c,0xe2,0x56,0x55,0xd5,0x2b,0x21,0x79,0x3c,0xcc,0x39,0x67,0x74,0x18,
0xfe,0x70,0xed,0x3b,0xf0,0x66,0xe6,0xc5,0x8d,0xc3,0x11,0x92,0x7b,0x0c,0x81,0x19,
0xd4,0xa8,0xdd,0xd0,0x1e,0xe1,0xdf,0xd3,0x63,0xd1,0xc0,0x07,0x71,0xc7,0xaf,0x72,
0x16,0x77,0x80,0xfb,0x51,0x85,0x0f,0xe0,0x6a,0x36,0xd2,0x87,0x8a,0xc2,0xbb,0xde,
0x88,0x12,0x63,0x56,0x90,0xb2,0xc0,0xf4,0x44,0xf0,0x81,0x54,0x07,0x5a,0x73,0xf4,
0x5d,0x12,0x8b,0xac,0xeb,0x0c,0x70,0x3a,0x4c,0x81,0xf1,0x1f,0x70,0x2d,0xbd,0x4c,
0x08,0xf0,0xce,0x0c,0xad,0xbf,0x88,0xb2,0xe2,0xe8,0x06,0x33,0x3f,0x5a,0xbb,0x18,
0x2f,0x30,0x47,0x37,0x94,0xe9,0xc5,0x7d,0x18,0x41,0xc2,0x94,0x04,0x32,0xb7,0xd9,
0x06,0x8b,0x7f,0xef,0xcc,0x11,0xca,0x69,0xea,0x9c,0x4a,0x1e,0xd0,0xb5,0x68,0x3f,
0xdc,0x04,0xd1,0x4d,0x2a,0x67,0x75,0x71,0x6f,0x46,0x17,0xea,0x62,0xd4,0xeb,0xb2,
0xf6,0x5b,0xcd,0xf3,0xd3,0x9f,0x60,0xce,0x36,0x55,0xa5,0x4b,0xab,0x15,0x61,0x15,
0x65,0x95,0x3c,0xd3,0x86,0x49,0x2d,0xcb,0xca,0x36,0x76,0xcf,0x88,0xad,0x4b,0xab,
0xaa,0x9a,0x35,0x1c,0xfd,0x68,0xb4,0x79,0xb9,0xfa,0x59,0x2a,0xff,0xeb,0x84,0x9d,
0xde,0xd3,0xb3,0x3a,0xab,0x3d,0xd3,0x95,0x65,0x67,0x4b,0xa5,0x0d,0xbd,0x69,0xd3,
0x34,0xd8,0xd0,0x5d,0x59,0xef,0xa8,0x35,0x58,0x31,0xc6,0xd1,0xcf,0x4e,0x79,0x77,
0xe8,0x73,0x79,0x5c,0xfd,0xf2,0x08,0xe4,0x25,0xbc,0xc2,0xb6,0x7d,0xc0,0xfb,0x04,
0x09,0xdb,0xe3,0x02,0x93,0x02,0x97,0x27,0x8c,0x0f,0xf1,0x44,0x59,0xff,0x25,0x93,
0xff,0x91,0xc9,0x2f,0x59,0x3b,0x8f,0xeb,0x24,0xd4,0x6a,0x38,0xca,0x30,0xb3,0x4e,
0x0b,0x82,0x31,0x09,0x64,0x40,0x89,0x5a,0x17,0x33,0x67,0x5d,0x42,0x99,0xfb,0x94,
0xad,0x9b,0xac,0x1f,0xb5,0x11,0xb8,0xa6,0x34,0x98,0x89,0x30,0x91,0xfe,0x7d,0x32,
0xa0,0x73,0xc3,0xeb,0x11,0x8e,0xb3,0x6b,0xdd,0x20,0x3b,0x28,0x2e,0x72,0xd6,0x91,
0xe3,0x28,0x5e,0x67,0xe1,0x20,0x83,0x2f,0x1b,0xfa,0x25,0x10,0xc3,0x86,0x62,0xda,
0x62,0x64,0x51,0xca,0x2c,0x47,0x29,0xc1,0xff,0x00,0x00,0x00,0xff,0xff,0x03,0x00,
0xf1,0x18,0xdc,0x71,0x78,0xda,0xca,0x48,0xcd,0xc9,0xc9,0xcf,0x80,0x13,0x5c,0x00,
0x00,0x00,0x00,0xff,0xff,0x03,0x00,0x37,0xf7,0x06,0x47
};

enum enc {
    GZIP,
    BZIP2
};

static void verify(unsigned char *d, size_t s,
    void (*f1)(struct transform *, struct transform_entry *),
    void (*f2)(struct transform *, struct transform_entry *),
    enum enc etype)
{
	struct transform_entry *ae;
	struct transform *a;
	unsigned char *buff;
	int r;

	assert((a = transform_read_new()) != NULL);
	switch (etype) {
	case BZIP2:
		/* This is only check whether bzip is supported or not.
		 * This filter won't be used this test.  */
		if (ARCHIVE_OK != transform_read_support_compression_bzip2(a)) {
			skipping("Unsupported bzip2");
			assertEqualInt(ARCHIVE_OK, transform_read_free(a));
			return;
		}
		break;
	case GZIP:
		/* This gzip must be needed. transform_read_support_format_xar()
		 * will return a warning if gzip is unsupported. */
		break;
	}
	assertA(0 == transform_read_support_compression_all(a));
	r = transform_read_support_format_xar(a);
	if (r == ARCHIVE_WARN) {
		skipping("xar reading not fully supported on this platform");
		assertEqualInt(ARCHIVE_OK, transform_read_free(a));
		return;
	}
	assert((buff = malloc(100000)) != NULL);
	if (buff == NULL)
		return;
	memcpy(buff, d, s);
	memset(buff + s, 0, 2048);

	assertA(0 == transform_read_support_format_all(a));
	assertA(0 == transform_read_open_memory(a, buff, s + 1024));
	assertA(0 == transform_read_next_header(a, &ae));
	assertEqualInt(archive_compression(a), ARCHIVE_FILTER_NONE);
	assertEqualInt(archive_format(a), ARCHIVE_FORMAT_XAR);
	/* Verify the only entry. */
	f1(a, ae);
	if (f2) {
		assertA(0 == transform_read_next_header(a, &ae));
		assertEqualInt(archive_compression(a), ARCHIVE_FILTER_NONE);
		assertEqualInt(archive_format(a), ARCHIVE_FORMAT_XAR);
		/* Verify the only entry. */
		f2(a, ae);
	}
	/* End of archive. */
	assertEqualInt(ARCHIVE_EOF, transform_read_next_header(a, &ae));

	assertEqualIntA(a, ARCHIVE_OK, transform_read_close(a));
	assertEqualInt(ARCHIVE_OK, transform_read_free(a));
	free(buff);
}

DEFINE_TEST(test_read_format_xar)
{
	verify(archive1, sizeof(archive1), verify0, verify1, GZIP);
	verify(archive2, sizeof(archive2), verify0, verify2, GZIP);
	verify(archive3, sizeof(archive3), verify3, NULL, GZIP);
	verify(archive4, sizeof(archive4), verify4, NULL, GZIP);
	verify(archive5, sizeof(archive5), verify5, NULL, GZIP);
	verify(archive6, sizeof(archive6), verify6, NULL, GZIP);
	verify(archive7, sizeof(archive7), verify7, NULL, GZIP);
	verify(archive8, sizeof(archive8), verify0, NULL, BZIP2);
	verify(archive9, sizeof(archive9), verify0, NULL, GZIP);
	verify(archive10, sizeof(archive10), verify0, NULL, GZIP);
	verify(archive11, sizeof(archive11), verify0, NULL, GZIP);
}

