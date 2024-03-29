/*-
 * Copyright (c) 2012 Michihiro NAKAJIMA
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

static void
test_exclusion_mbs(void)
{
	struct archive_entry *ae;
	struct archive *m;

	if (!assert((m = archive_matching_new()) != NULL))
		return;
	if (!assert((ae = archive_entry_new()) != NULL)) {
		archive_matching_free(m);
		return;
	}

	/* Test for pattern "^aa*" */
	assertEqualIntA(m, 0, archive_matching_exclude_pattern(m, "^aa*"));

	/* Test with 'aa1234', which should be excluded. */
	archive_entry_copy_pathname(ae, "aa1234");
	failure("'aa1234' should be excluded");
	assertEqualInt(1, archive_matching_path_excluded(m, ae));
	assertEqualInt(1, archive_matching_excluded(m, ae));
	archive_entry_clear(ae);
	archive_entry_copy_pathname_w(ae, L"aa1234");
	failure("'aa1234' should be excluded");
	assertEqualInt(1, archive_matching_path_excluded(m, ae));
	assertEqualInt(1, archive_matching_excluded(m, ae));

	/* Test with 'a1234', which should not be excluded. */
	archive_entry_copy_pathname(ae, "a1234");
	failure("'a1234' should not be excluded");
	assertEqualInt(0, archive_matching_path_excluded(m, ae));
	assertEqualInt(0, archive_matching_excluded(m, ae));
	archive_entry_clear(ae);
	archive_entry_copy_pathname_w(ae, L"a1234");
	failure("'a1234' should not be excluded");
	assertEqualInt(0, archive_matching_path_excluded(m, ae));
	assertEqualInt(0, archive_matching_excluded(m, ae));

	/* Clean up. */
	archive_entry_free(ae);
	archive_matching_free(m);
}

static void
test_exclusion_wcs(void)
{
	struct archive_entry *ae;
	struct archive *m;

	if (!assert((m = archive_matching_new()) != NULL))
		return;
	if (!assert((ae = archive_entry_new()) != NULL)) {
		archive_matching_free(m);
		return;
	}

	/* Test for pattern "^aa*" */
	assertEqualIntA(m, 0, archive_matching_exclude_pattern_w(m, L"^aa*"));

	/* Test with 'aa1234', which should be excluded. */
	archive_entry_copy_pathname(ae, "aa1234");
	failure("'aa1234' should be excluded");
	assertEqualInt(1, archive_matching_path_excluded(m, ae));
	assertEqualInt(1, archive_matching_excluded(m, ae));
	archive_entry_clear(ae);
	archive_entry_copy_pathname_w(ae, L"aa1234");
	failure("'aa1234' should be excluded");
	assertEqualInt(1, archive_matching_path_excluded(m, ae));
	assertEqualInt(1, archive_matching_excluded(m, ae));

	/* Test with 'a1234', which should not be excluded. */
	archive_entry_copy_pathname(ae, "a1234");
	failure("'a1234' should not be excluded");
	assertEqualInt(0, archive_matching_path_excluded(m, ae));
	assertEqualInt(0, archive_matching_excluded(m, ae));
	archive_entry_clear(ae);
	archive_entry_copy_pathname_w(ae, L"a1234");
	failure("'a1234' should not be excluded");
	assertEqualInt(0, archive_matching_path_excluded(m, ae));
	assertEqualInt(0, archive_matching_excluded(m, ae));

	/* Clean up. */
	archive_entry_free(ae);
	archive_matching_free(m);
}

static void
test_inclusion_mbs(void)
{
	struct archive_entry *ae;
	struct archive *m;
	const char *mp;

	if (!assert((m = archive_matching_new()) != NULL))
		return;
	if (!assert((ae = archive_entry_new()) != NULL)) {
		archive_matching_free(m);
		return;
	}

	/* Test for pattern "^aa*" */
	assertEqualIntA(m, 0, archive_matching_include_pattern(m, "^aa*"));

	/* Test with 'aa1234', which should not be excluded. */
	archive_entry_copy_pathname(ae, "aa1234");
	failure("'aa1234' should not be excluded");
	assertEqualInt(0, archive_matching_path_excluded(m, ae));
	assertEqualInt(0, archive_matching_excluded(m, ae));
	archive_entry_clear(ae);
	archive_entry_copy_pathname_w(ae, L"aa1234");
	failure("'aa1234' should not be excluded");
	assertEqualInt(0, archive_matching_path_excluded(m, ae));
	assertEqualInt(0, archive_matching_excluded(m, ae));

	/* Test with 'a1234', which should be excluded. */
	archive_entry_copy_pathname(ae, "a1234");
	failure("'a1234' should be excluded");
	assertEqualInt(1, archive_matching_path_excluded(m, ae));
	assertEqualInt(1, archive_matching_excluded(m, ae));
	archive_entry_clear(ae);
	archive_entry_copy_pathname_w(ae, L"a1234");
	failure("'a1234' should be excluded");
	assertEqualInt(1, archive_matching_path_excluded(m, ae));
	assertEqualInt(1, archive_matching_excluded(m, ae));

	/* Verify unmatched_inclusions. */
	assertEqualInt(0, archive_matching_path_unmatched_inclusions(m));
	assertEqualIntA(m, ARCHIVE_EOF,
	    archive_matching_path_unmatched_inclusions_next(m, &mp));

	/* Clean up. */
	archive_entry_free(ae);
	archive_matching_free(m);
}

static void
test_inclusion_wcs(void)
{
	struct archive_entry *ae;
	struct archive *m;
	const char *mp;

	if (!assert((m = archive_matching_new()) != NULL))
		return;
	if (!assert((ae = archive_entry_new()) != NULL)) {
		archive_matching_free(m);
		return;
	}

	/* Test for pattern "^aa*" */
	assertEqualIntA(m, 0, archive_matching_include_pattern_w(m, L"^aa*"));

	/* Test with 'aa1234', which should not be excluded. */
	archive_entry_copy_pathname(ae, "aa1234");
	failure("'aa1234' should not be excluded");
	assertEqualInt(0, archive_matching_path_excluded(m, ae));
	assertEqualInt(0, archive_matching_excluded(m, ae));
	archive_entry_clear(ae);
	archive_entry_copy_pathname_w(ae, L"aa1234");
	failure("'aa1234' should not be excluded");
	assertEqualInt(0, archive_matching_path_excluded(m, ae));
	assertEqualInt(0, archive_matching_excluded(m, ae));

	/* Test with 'a1234', which should be excluded. */
	archive_entry_copy_pathname(ae, "a1234");
	failure("'a1234' should be excluded");
	assertEqualInt(1, archive_matching_path_excluded(m, ae));
	assertEqualInt(1, archive_matching_excluded(m, ae));
	archive_entry_clear(ae);
	archive_entry_copy_pathname_w(ae, L"a1234");
	failure("'a1234' should be excluded");
	assertEqualInt(1, archive_matching_path_excluded(m, ae));
	assertEqualInt(1, archive_matching_excluded(m, ae));

	/* Verify unmatched_inclusions. */
	assertEqualInt(0, archive_matching_path_unmatched_inclusions(m));
	assertEqualIntA(m, ARCHIVE_EOF,
	    archive_matching_path_unmatched_inclusions_next(m, &mp));

	/* Clean up. */
	archive_entry_free(ae);
	archive_matching_free(m);
}

static void
test_exclusion_and_inclusion(void)
{
	struct archive_entry *ae;
	struct archive *m;
	const char *mp;
	const wchar_t *wp;

	if (!assert((m = archive_matching_new()) != NULL))
		return;
	if (!assert((ae = archive_entry_new()) != NULL)) {
		archive_matching_free(m);
		return;
	}

	assertEqualIntA(m, 0, archive_matching_exclude_pattern(m, "^aaa*"));
	assertEqualIntA(m, 0, archive_matching_include_pattern_w(m, L"^aa*"));
	assertEqualIntA(m, 0, archive_matching_include_pattern(m, "^a1*"));

	/* Test with 'aa1234', which should not be excluded. */
	archive_entry_copy_pathname(ae, "aa1234");
	failure("'aa1234' should not be excluded");
	assertEqualInt(0, archive_matching_path_excluded(m, ae));
	assertEqualInt(0, archive_matching_excluded(m, ae));
	archive_entry_clear(ae);
	archive_entry_copy_pathname_w(ae, L"aa1234");
	failure("'aa1234' should not be excluded");
	assertEqualInt(0, archive_matching_path_excluded(m, ae));
	assertEqualInt(0, archive_matching_excluded(m, ae));

	/* Test with 'aaa1234', which should be excluded. */
	archive_entry_copy_pathname(ae, "aaa1234");
	failure("'aaa1234' should be excluded");
	assertEqualInt(1, archive_matching_path_excluded(m, ae));
	assertEqualInt(1, archive_matching_excluded(m, ae));
	archive_entry_clear(ae);
	archive_entry_copy_pathname_w(ae, L"aaa1234");
	failure("'aaa1234' should be excluded");
	assertEqualInt(1, archive_matching_path_excluded(m, ae));
	assertEqualInt(1, archive_matching_excluded(m, ae));

	/* Verify unmatched_inclusions. */
	assertEqualInt(1, archive_matching_path_unmatched_inclusions(m));
	/* Verify unmatched inclusion patterns. */
	assertEqualIntA(m, ARCHIVE_OK,
	    archive_matching_path_unmatched_inclusions_next(m, &mp));
	assertEqualString("^a1*", mp);
	assertEqualIntA(m, ARCHIVE_EOF,
	    archive_matching_path_unmatched_inclusions_next(m, &mp));
	/* Verify unmatched inclusion patterns again in Wide-Char. */
	assertEqualIntA(m, ARCHIVE_OK,
	    archive_matching_path_unmatched_inclusions_next_w(m, &wp));
	assertEqualWString(L"^a1*", wp);
	assertEqualIntA(m, ARCHIVE_EOF,
	    archive_matching_path_unmatched_inclusions_next_w(m, &wp));

	/* Clean up. */
	archive_entry_free(ae);
	archive_matching_free(m);
}

DEFINE_TEST(test_archive_matching_path)
{
	test_exclusion_mbs();
	test_exclusion_wcs();
	test_inclusion_mbs();
	test_inclusion_wcs();
	test_exclusion_and_inclusion();
}
