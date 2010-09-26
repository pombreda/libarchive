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

#include "transform_platform.h"
__FBSDID("$FreeBSD$");

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "transform_string.h"
#include "transform_write_private.h"

/*
 * See if a filter can handle this option.
 */
int
transform_write_set_filter_option(struct transform *_a, const char *name,
    const char *key, const char *value)
{
	struct transform_write *a = (struct transform_write *)_a;
	struct transform_write_filter *filter;
	int r, handled = 0;

	transform_check_magic(_a, TRANSFORM_WRITE_MAGIC, TRANSFORM_STATE_NEW,
		"transform_write_set_filter_options");

	for (filter = a->filter_first; filter != NULL; filter = filter->base.upstream.write) {
		if (filter->options == NULL)
			continue;
		if (name != NULL && strcmp(name, filter->base.name) != 0)
			continue;
		r = filter->options(_a, filter->base.data, key, value);
		if (r == TRANSFORM_FATAL)
			return (r);
		if (r == TRANSFORM_OK)
			++handled;
	}
	return (handled > 0 ? TRANSFORM_OK : TRANSFORM_WARN);
}
