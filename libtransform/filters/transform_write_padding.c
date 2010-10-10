/*-
 * Copyright (c) 2003-2010 Tim Kientzle
 * Copyright (c) 2003-2010 Brian Harring
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

#include "transform_platform.h"
__FBSDID("$FreeBSD: head/lib/libtransform/transform_write.c 201099 2009-12-28 03:03:00Z kientzle $");

#include "transform.h"
#include "transform_private.h"

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

struct padding_data {
	int64_t block_size;
	int64_t position;
	int     is_dynamic;
};

#define NULL_ARRAY_LENGTH 1024
static char nulls[NULL_ARRAY_LENGTH] = {0};

static int common_padding_init(struct transform *t, int64_t block_size, int is_dynamic);
static ssize_t padding_write(struct transform *,
	void *, const void *, size_t, struct transform_write_filter *);
static int padding_close(struct transform *_t, void *,
	struct transform_write_filter *);
static int padding_free(struct transform *_t, void *);

int
transform_write_add_filter_padding(struct transform *t, int64_t block_size)
{
	block_size = block_size <= 0 ? DEFAULT_WRITE_BLOCK_SIZE : block_size;
	return (common_padding_init(t, block_size, 0));
}

int
transform_write_add_filter_dynamic_padding(struct transform *t)
{
	return (common_padding_init(t, 0, 1));
}

static int
common_padding_init(struct transform *t, int64_t block_size, int is_dynamic)
{
	struct padding_data *pdata = calloc(1, sizeof(struct padding_data));
	if (!pdata) {
		transform_set_error(t, ENOMEM, "failed allocating padding data");
		return (TRANSFORM_FATAL);
	}
	pdata->block_size = block_size;
	pdata->is_dynamic = is_dynamic;

	if (TRANSFORM_OK != transform_write_add_filter(t,
		pdata, "padding", TRANSFORM_FILTER_NONE,
		NULL, // probably should add options to simplify controling this
		NULL,
		padding_write,
		padding_close,
		padding_free,
		0)) {
		free(pdata);
		return (TRANSFORM_FATAL);
	}
	return (TRANSFORM_OK);
}

static ssize_t
padding_write(struct transform *_t,
	void *_data, const void *buff, size_t length,
	struct transform_write_filter *upstream)
{
	struct padding_data *pdata = (struct padding_data *)_data;
	ssize_t written;

	written = transform_write_filter_output(upstream, buff, length);
	if (TRANSFORM_OK == written || TRANSFORM_WARN == written) {
		pdata->position += length;
		return (length);
	}
	return (written);
}

static int
padding_close(struct transform *t, void *_data,
	struct transform_write_filter *upstream)
{
	struct padding_data *pdata = (struct padding_data *)_data;
	int64_t remaining;
	int64_t bytes_in_last = 0;
	ssize_t written, desired;

	if (pdata->is_dynamic) {
		pdata->block_size = transform_write_get_bytes_per_block(t);
	}

	if (!pdata->block_size) {
		return (TRANSFORM_OK);
	}
	remaining = pdata->position % pdata->block_size;

	/* if it aligned on a block boundary alreay, nothing to do.
	 * if there hasn't been anydata, force nulls to a block boundary
	 */
	if (!remaining && pdata->position) {
		return (TRANSFORM_OK);
	}

	if (pdata->is_dynamic) {
		bytes_in_last = transform_write_get_bytes_in_last_block(t);
		if (bytes_in_last == 1) {
			/* no padding */
			return (TRANSFORM_OK);
		} else if (bytes_in_last <= 0) {
			bytes_in_last = pdata->block_size;
		}
		remaining = bytes_in_last - remaining;
	} else {
		remaining = pdata->block_size - remaining;
	}

	while (remaining) {
		desired = remaining > NULL_ARRAY_LENGTH ? NULL_ARRAY_LENGTH : remaining;
		written = transform_write_filter_output(upstream, nulls, desired);
		if (written < 0) {
			return (written);
		}
		if (TRANSFORM_OK != written) {
			if (!transform_errno(t)) {
				transform_set_error(t, EINVAL,
					"asked for %d bytes to be written, got just %d",
					(int)desired, (int)written);
			}
			return (TRANSFORM_FATAL);
		}
		remaining -= desired;
	}
	return (TRANSFORM_OK);
}

static int
padding_free(struct transform *t, void *_data)
{
	struct padding_data *pdata = (struct padding_data *)_data;

	free(pdata);
	return (TRANSFORM_OK);
}
