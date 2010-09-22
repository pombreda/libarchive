/*
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

void
logtransforminfo(struct transform *t)
{
	int x = 0, count;
	count = transform_filter_count(t);
	logprintf("  state of the transform; %i, %s\n", transform_errno(t),
		transform_error_string(t));
	logprintf("  transform configuration:\n");
	while (x < count) {
		logprintf("    transform filter #%i, code %i, name: %s\n",
			x, transform_filter_code(t, x), transform_filter_name(t, x));
		x++;
	}
}

int
assertion_transform_contents_mem(struct transform *t, const void *desired, int64_t size)
{
	const void *position = desired;
	int64_t remaining = size;
	const void *data;
	ssize_t avail;

	assertion_count(test_filename, test_line);

	while(remaining > 0) {
		int ret;
		data = transform_read_ahead(t, 1, &avail);
		assert(NULL != data);
		if(NULL == data) {
			break;
		}
		ret = memcmp(position, data, avail);
		if(ret == 0) {
			transform_read_consume(t, avail);
			position += avail;
			remaining -= avail;
			continue;
		}
		int x = 0;
		for(x=0; x < avail; x++) {
			if(((char *)position)[x] != ((char *)data)[x]) {
				break;
			}
		}
			
		failure_start(test_filename, test_line,
			"transform results not identical in the range of %i %i starting at %i, absolute offset %i",
			(position - desired), (position - desired + avail), (position - desired + x),
			transform_read_bytes_consumed(t));
		logtransforminfo(t);
		logprintf("  dump of desired block\n");
		loghexdump(position, NULL, avail, 0);
		logprintf("  dump of returned block\n");
		loghexdump(data, NULL, avail, 0);
		failure_finish(NULL);
		return 0;
	}
	assert(remaining == 0);

	if(remaining == 0)
		return 1;

	failure_start(test_filename, test_line,
		"transform comparison had a non zero remaining: %li", remaining);
	failure_finish(NULL);
	return 1;
}
