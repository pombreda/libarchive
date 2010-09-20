/*-
 * Copyright (c) 2010 Brian Harring
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
__FBSDID("$FreeBSD: asdf$");

struct my_data {
	size_t alignment;
	size_t required_buff;
	int read_invokes;
	int skip_invokes;
	void *buff;
};

int
my_read(struct transform *t, void *_private, struct transform_read_filter *upstream,
	const void **buff, size_t *bytes_read)
{
	struct my_data *self = (struct my_data *)_private;

	failure_context("my_read: bytes_read(%u), alignment(%u), required(%u), given_buff(%p), "
		"my_buff(%p)", (*bytes_read), self->alignment, self->required_buff, *buff,
		self->buff);

	if (*bytes_read) {
		assertEqualInt(0, *bytes_read % self->alignment);
		assert(*bytes_read >= self->required_buff);
	}
	if (self->buff) {
		assert(*buff == NULL);
		*buff = self->buff;
		*bytes_read = self->required_buff;
	} else {
		assert(NULL != *buff);
		memset((void *)*buff, 0, *bytes_read);
	}
	self->read_invokes++;
	failure_context(NULL);
	return (TRANSFORM_OK);
}

int64_t
my_skip(struct transform *t, void *_private, struct transform_read_filter *upstream,
	int64_t length)
{
	struct my_data *self = (struct my_data *)_private;

	failure_context("my_skip: alignment(%u), required(%u), length(%u), "
		"my_buff(%d)", self->alignment, self->required_buff, length);
	assert(length > 0);
	assertEqualInt(0, length % self->alignment);
	self->skip_invokes++;
	failure_context(NULL);
	return (length);
}

struct transform *
setup_transform(struct my_data *d, size_t alignment, size_t req_buff, int has_skip)
{
	struct transform *t = transform_read_new();
	struct transform_read_filter *filter = transform_read_filter_new(d,
		"", TRANSFORM_FILTER_NONE, my_read,
			has_skip ? my_skip : NULL, NULL, NULL,
			d->buff ? TRANSFORM_FILTER_SELF_BUFFERING : 0 |
			TRANSFORM_FILTER_SOURCE);
	d->alignment = alignment;
	d->required_buff =  req_buff;
	assert(NULL != filter);
	assert(NULL != t);
	if (filter) {
		if (req_buff) {
			assertEqualInt(TRANSFORM_OK, transform_read_filter_set_buffering(filter, req_buff));
		}
		if (alignment) {
			assertEqualInt(TRANSFORM_OK, transform_read_filter_set_alignment(filter, alignment));
		}
	} else {
		if (t) {
			transform_read_free(t);
		}
		return NULL;
	}
	if (t) {
		int ret = transform_read_open_source(t, filter);
		assertEqualIntA(t, ret, TRANSFORM_OK);
		if (TRANSFORM_OK == ret) {
			return t;
		}
		transform_read_free(t);
	}
	return NULL;
}

void
read_testing(int has_skip, int self_buffering)
{
	struct transform *t;
	struct my_data data;
	void *my_alloc = NULL;
	if (self_buffering) {
		my_alloc = calloc(1, 32 * 1024);
		assert(NULL != my_alloc);
		if (!my_alloc) {
			return;
		}
	}
	const void *buff = NULL;
	ssize_t avail;
	memset(&data, 0, sizeof(data));
	(void)avail;
	(void)buff;
	if (!(t = setup_transform(&data, 1024, 2048, has_skip))) {
		return;
	}
	assert(NULL != (buff = transform_read_ahead(t, 1, &avail)));
	assert(NULL != (buff = transform_read_ahead(t, 4096, &avail)));
	assertEqualInt(avail, 4096);
	assertEqualIntA(t, 4096, transform_read_consume(t, 4096));
	assertEqualInt(data.read_invokes, 2);
	assertEqualInt(data.skip_invokes, 0);
	assertEqualIntA(t, 4096, transform_read_consume(t, 4096));
	if (has_skip) {
		assertEqualInt(data.skip_invokes, 1);
	} else {
		assertEqualInt(data.read_invokes, 4);
	}
	assertEqualIntA(t, 4093, transform_read_consume(t, 4093));
	if (has_skip) {
		assertEqualInt(data.skip_invokes, 2);
		assertEqualInt(data.read_invokes, 3);
	} else {
		assertEqualInt(data.read_invokes, 6);
	}
	assert(NULL != transform_read_ahead(t, 1, &avail));
	/* realign */
	if (avail)
		assertEqualIntA(t, avail, transform_read_consume(t, avail));
	buff = transform_read_ahead(t, (16 * 1024) + 1, &avail);
	/* client side, it doesn't necessarily have to align, but the my_read
	 * implementation's assertions will spot if libtransform asks for
	 * a misalignment at that layer.
	 */
	assert(avail >= (16 * 1024) +1);
	/* just a sanity check for the implementation */
	if (!has_skip) {
		assertEqualInt(data.skip_invokes, 0);
	}
	transform_read_free(t);
	free(my_alloc);
}
	


/* Verify that attempting to open an invalid fd returns correct error. */
DEFINE_TEST(test_alignment)
{
	read_testing(0, 0);
	read_testing(1, 0);
	read_testing(1, 1);
	read_testing(0, 1);
}
