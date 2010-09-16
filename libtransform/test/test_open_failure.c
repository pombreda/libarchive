/*-
 * Copyright (c) 2003-2010 Tim Kientzle
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

#define MAGIC 123456789
struct my_data {
	int magic;
	int read_return;
	int read_called;
	int write_return;
	int write_called;
	int open_return;
	int open_called;
	int close_return;
	int close_called;
	size_t bytes_read;
	char *buff;
};

static int
my_read(struct transform *a, void *_private, struct transform_read_filter *upstream,
	const void **buff, size_t *bytes_read)
{
	struct my_data *private = (struct my_data *)_private;
	assertEqualInt(MAGIC, private->magic);
	++private->read_called;
	*bytes_read = private->bytes_read;
	*buff = private->buff;
	return (private->read_return);
}

static ssize_t
my_write(struct transform *a, void *_private, const void *buff, size_t s)
{
	struct my_data *private = (struct my_data *)_private;
	assertEqualInt(MAGIC, private->magic);
	++private->write_called;
	return (private->write_return);
}

static int
my_open(struct transform *a, void *_private)
{
	struct my_data *private = (struct my_data *)_private;
	assertEqualInt(MAGIC, private->magic);
	++private->open_called;
	return (private->open_return);
}

static int
my_close(struct transform *a, void *_private)
{
	struct my_data *private = (struct my_data *)_private;
	assertEqualInt(MAGIC, private->magic);
	++private->close_called;
	return (private->close_return);
}

DEFINE_TEST(test_open_failure)
{
	struct transform *a;
	struct my_data private;

	memset(&private, 0, sizeof(private));
	private.magic = MAGIC;
	private.open_return = TRANSFORM_OK;
	a = transform_read_new();
	assert(a != NULL);
	assertEqualInt(TRANSFORM_OK,
	    transform_read_open(a, &private, my_read, NULL, my_close));
	assertEqualInt(0, private.read_called);
	assertEqualInt(0, private.close_called);

	memset(&private, 0, sizeof(private));
	private.magic = MAGIC;
	private.read_return = TRANSFORM_EOF;
	private.bytes_read = 5;
	private.buff = "asdf";
	a = transform_read_new();
	assert(a != NULL);
	assertEqualInt(TRANSFORM_OK,
	    transform_read_open(a, &private, my_read, NULL, my_close));
	transform_read_consume(a, 2);
	transform_read_consume(a, 2);
	assertEqualInt(TRANSFORM_FATAL,
		transform_read_consume(a, 2));
	assertEqualInt(1, private.read_called);
	assertEqualInt(0, private.close_called);


	memset(&private, 0, sizeof(private));
	private.magic = MAGIC;
	private.read_return = TRANSFORM_FATAL;
	a = transform_read_new();
	assert(a != NULL);
	assertEqualInt(TRANSFORM_OK,
	    transform_read_add_compress(a));

	memset(&private, 0, sizeof(private));
	private.magic = MAGIC;
	private.open_return = TRANSFORM_FATAL;
	a = transform_write_new();
	assert(a != NULL);
	assertEqualInt(TRANSFORM_FATAL,
	    transform_write_open(a, &private, my_open, my_write, my_close));
	assertEqualInt(1, private.open_called);
	assertEqualInt(0, private.write_called);
	assertEqualInt(1, private.close_called);

	memset(&private, 0, sizeof(private));
	private.magic = MAGIC;
	private.open_return = TRANSFORM_FATAL;
	a = transform_write_new();
	assert(a != NULL);
	transform_write_add_filter_compress(a);
	assertEqualInt(TRANSFORM_FATAL,
	    transform_write_open(a, &private, my_open, my_write, my_close));
	assertEqualInt(1, private.open_called);
	assertEqualInt(0, private.write_called);
	assertEqualInt(1, private.close_called);

	memset(&private, 0, sizeof(private));
	private.magic = MAGIC;
	private.open_return = TRANSFORM_FATAL;
	a = transform_write_new();
	assert(a != NULL);
	transform_write_add_filter_gzip(a);
	assertEqualInt(TRANSFORM_FATAL,
	    transform_write_open(a, &private, my_open, my_write, my_close));
	assertEqualInt(1, private.open_called);
	assertEqualInt(0, private.write_called);
	assertEqualInt(1, private.close_called);

}
