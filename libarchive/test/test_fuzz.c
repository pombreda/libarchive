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
__FBSDID("$FreeBSD: head/lib/libarchive/test/test_fuzz.c 201247 2009-12-30 05:59:21Z kientzle $");

/*
 * This was inspired by an ISO fuzz tester written by Michal Zalewski
 * and posted to the "vulnwatch" mailing list on March 17, 2005:
 *    http://seclists.org/vulnwatch/2005/q1/0088.html
 *
 * This test simply reads each archive image into memory, pokes
 * random values into it and runs it through libarchive.  It tries
 * to damage about 1% of each file and repeats the exercise 100 times
 * with each file.
 *
 * Unlike most other tests, this test does not verify libarchive's
 * responses other than to ensure that libarchive doesn't crash.
 *
 * Due to the deliberately random nature of this test, it may be hard
 * to reproduce failures.  Because this test deliberately attempts to
 * induce crashes, there's little that can be done in the way of
 * post-failure diagnostics.
 */

/* Because this works for any archive, we can just re-use the archives
 * developed for other tests. */
struct files {
	int uncompress; /* If 1, decompress the file before fuzzing. */
	const char **names;
};
static const char *fileset1[] =
{
	"test_fuzz_1.iso.Z", /* Exercise compress decompressor. */
	NULL
};
static const char *fileset2[] =
{
	"test_fuzz_1.iso.Z",
	NULL
};
static const char *fileset3[] =
{
	"test_fuzz.cab",
	NULL
};
static const char *fileset4[] =
{
	"test_fuzz.lzh",
	NULL
};
static const char *fileset5[] =
{
	"test_compat_bzip2_1.tbz", /* Exercise bzip2 decompressor. */
	NULL
};
static const char *fileset6[] =
{
	"test_compat_bzip2_1.tbz",
	NULL
};
static const char *fileset7[] =
{
	"test_compat_gtar_1.tar",
	NULL
};
static const char *fileset8[] =
{
	"test_compat_gzip_1.tgz", /* Exercise gzip decompressor. */
	NULL
};
static const char *fileset9[] =
{
	"test_compat_gzip_2.tgz", /* Exercise gzip decompressor. */
	NULL
};
static const char *fileset10[] =
{
	"test_compat_tar_hardlink_1.tar",
	NULL
};
static const char *fileset11[] =
{
	"test_compat_xz_1.txz", /* Exercise xz decompressor. */
	NULL
};
static const char *fileset12[] =
{
	"test_compat_zip_1.zip",
	NULL
};
static const char *fileset13[] =
{
	"test_read_format_7zip_bzip2.7z",
	NULL
};
static const char *fileset14[] =
{
	"test_read_format_7zip_bcj_lzma1.7z",
	NULL
};
static const char *fileset15[] =
{
	"test_read_format_7zip_bcj_lzma2.7z",
	NULL
};
static const char *fileset16[] =
{
	"test_read_format_7zip_bcj2_lzma1_1.7z",
	NULL
};
static const char *fileset17[] =
{
	"test_read_format_7zip_bcj2_lzma1_2.7z",
	NULL
};
static const char *fileset18[] =
{
	"test_read_format_7zip_bcj2_lzma2_1.7z",
	NULL
};
static const char *fileset19[] =
{
	"test_read_format_7zip_bcj2_lzma2_2.7z",
	NULL
};
static const char *fileset20[] =
{
	"test_read_format_7zip_copy.7z",
	NULL
};
static const char *fileset21[] =
{
	"test_read_format_7zip_deflate.7z",
	NULL
};
static const char *fileset22[] =
{
	"test_read_format_7zip_lzma1.7z",
	NULL
};
static const char *fileset23[] =
{
	"test_read_format_7zip_lzma1_lzma2.7z",
	NULL
};
static const char *fileset24[] =
{
	"test_read_format_7zip_ppmd.7z",
	NULL
};
static const char *fileset25[] =
{
	"test_read_format_ar.ar",
	NULL
};
static const char *fileset26[] =
{
	"test_read_format_cpio_bin_be.cpio",
	NULL
};
static const char *fileset27[] =
{
	"test_read_format_cpio_svr4_gzip_rpm.rpm", /* Test RPM unwrapper */
	NULL
};
static const char *fileset28[] =
{
	"test_read_format_rar.rar", /* Uncompressed RAR test */
	NULL
};
static const char *fileset29[] =
{
	"test_read_format_rar_binary_data.rar", /* RAR file with binary data */
	NULL
};
static const char *fileset30[] =
{
	"test_read_format_rar_compress_best.rar", /* Best Compressed RAR test */
	NULL
};
static const char *fileset31[] =
{
	"test_read_format_rar_compress_normal.rar", /* Normal Compressed RAR
	                                                  * test */
	NULL
};
static const char *fileset32[] =
{
	"test_read_format_rar_multi_lzss_blocks.rar", /* Normal Compressed Multi
	                                               * LZSS blocks RAR test */
	NULL
};
static const char *fileset33[] =
{
	"test_read_format_rar_noeof.rar", /* RAR with no EOF header */
	NULL
};
static const char *fileset34[] =
{
	"test_read_format_rar_ppmd_lzss_conversion.rar", /* Best Compressed
	                                                  * RAR file with both
	                                                  * PPMd and LZSS
	                                                  * blocks */
	NULL
};
static const char *fileset35[] =
{
	"test_read_format_rar_sfx.exe", /* RAR SFX archive */
	NULL
};
static const char *fileset36[] =
{
	"test_read_format_rar_subblock.rar", /* RAR with subblocks */
	NULL
};
static const char *fileset37[] =
{
	"test_read_format_rar_unicode.rar", /* RAR with Unicode filenames */
	NULL
};
static const char *fileset38[] =
{
	"test_read_format_gtar_sparse_1_17_posix10_modified.tar",
	NULL
};
static const char *fileset39[] =
{
	"test_read_format_mtree.mtree",
	NULL
};
static const char *fileset40[] =
{
	"test_read_format_tar_empty_filename.tar",
	NULL
};
static const char *fileset41[] =
{
	"test_read_format_zip.zip",
	NULL
};
static const char *fileset42[] =
{
	"test_read_format_rar_multivolume.part0001.rar",
	"test_read_format_rar_multivolume.part0002.rar",
	"test_read_format_rar_multivolume.part0003.rar",
	"test_read_format_rar_multivolume.part0004.rar",
	NULL
};

static const struct files filesets[] = {
	{0, fileset1},
	{1, fileset2},
	{0, fileset3},
	{0, fileset4},
	{0, fileset5},
	{1, fileset6},
	{0, fileset7},
	{0, fileset8},
	{0, fileset9},
	{0, fileset10},
	{0, fileset11},
	{0, fileset12},
	{0, fileset13},
	{0, fileset14},
	{0, fileset15},
	{0, fileset16},
	{0, fileset17},
	{0, fileset18},
	{0, fileset19},
	{0, fileset20},
	{0, fileset21},
	{0, fileset22},
	{0, fileset23},
	{0, fileset24},
	{0, fileset25},
	{0, fileset26},
	{0, fileset27},
	{0, fileset28},
	{0, fileset29},
	{0, fileset30},
	{0, fileset31},
	{0, fileset32},
	{0, fileset33},
	{0, fileset34},
	{0, fileset35},
	{0, fileset36},
	{0, fileset37},
	{0, fileset38},
	{0, fileset39},
	{0, fileset40},
	{0, fileset41},
	{0, fileset42},
	{1, NULL}
};

DEFINE_TEST(test_fuzz)
{
	const void *blk;
	size_t blk_size;
	int64_t blk_offset;
	int n;

	for (n = 0; filesets[n].names != NULL; ++n) {
		const size_t buffsize = 30000000;
		struct archive_entry *ae;
		struct archive *a;
		char *rawimage = NULL, *image = NULL, *tmp = NULL;
		size_t size, oldsize = 0;
		int i, q;

		extract_reference_files(filesets[n].names);
		if (filesets[n].uncompress) {
			int r;
			/* Use format_raw to decompress the data. */
			assert((a = archive_read_new()) != NULL);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_support_filter_all(a));
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_support_format_raw(a));
			r = archive_read_open_filenames(a, filesets[n].names, 16384);
			if (r != ARCHIVE_OK) {
				archive_read_free(a);
				if (filesets[n].names[0] == NULL || filesets[n].names[1] == NULL) {
					skipping("Cannot uncompress fileset");
				} else {
					skipping("Cannot uncompress %s", filesets[n].names[0]);
				}
				continue;
			}
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_next_header(a, &ae));
			rawimage = malloc(buffsize);
			size = archive_read_data(a, rawimage, buffsize);
			assertEqualIntA(a, ARCHIVE_EOF,
			    archive_read_next_header(a, &ae));
			assertEqualInt(ARCHIVE_OK,
			    archive_read_free(a));
			assert(size > 0);
			if (filesets[n].names[0] == NULL || filesets[n].names[1] == NULL) {
				failure("Internal buffer is not big enough for "
					"uncompressed test files");
			} else {
				failure("Internal buffer is not big enough for "
					"uncompressed test file: %s", filesets[n].names[0]);
			}
			if (!assert(size < buffsize)) {
				free(rawimage);
				continue;
			}
		} else {
			for (i = 0; filesets[n].names[i] != NULL; ++i)
			{
				tmp = slurpfile(&size, filesets[n].names[i]);
				rawimage = (char *)realloc(rawimage, oldsize + size);
				memcpy(rawimage + oldsize, tmp, size);
				oldsize += size;
				size = oldsize;
				free(tmp);
				if (!assert(rawimage != NULL))
					continue;
			}
		}
		image = malloc(size);
		assert(image != NULL);
		srand((unsigned)time(NULL));

		for (i = 0; i < 100; ++i) {
			FILE *f;
			int j, numbytes, trycnt;

			/* Fuzz < 1% of the bytes in the archive. */
			memcpy(image, rawimage, size);
			q = size / 100;
			if (!q) q = 1;
			numbytes = (int)(rand() % q);
			for (j = 0; j < numbytes; ++j)
				image[rand() % size] = (char)rand();

			/* Save the messed-up image to a file.
			 * If we crash, that file will be useful. */
			for (trycnt = 0; trycnt < 3; trycnt++) {
				f = fopen("after.test.failure.send.this.file."
				    "to.libarchive.maintainers.with.system.details", "wb");
				if (f != NULL)
					break;
#if defined(_WIN32) && !defined(__CYGWIN__)
				/*
				 * Sometimes previous close operation does not completely
				 * end at this time. So we should take a wait while
				 * the operation running.
				 */
				Sleep(100);
#endif
			}
			assertEqualInt((size_t)size, fwrite(image, 1, (size_t)size, f));
			fclose(f);

			assert((a = archive_read_new()) != NULL);
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_support_filter_all(a));
			assertEqualIntA(a, ARCHIVE_OK,
			    archive_read_support_format_all(a));

			if (0 == archive_read_open_memory(a, image, size)) {
				while(0 == archive_read_next_header(a, &ae)) {
					while (0 == archive_read_data_block(a,
						&blk, &blk_size, &blk_offset))
						continue;
				}
				archive_read_close(a);
			}
			archive_read_free(a);
		}
		free(image);
		free(rawimage);
	}
}


