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
 *
 * $FreeBSD: head/lib/libtransform/transform_private.h 201098 2009-12-28 02:58:14Z kientzle $
 */

#ifndef __LIBTRANSFORM_BUILD
#error This header is only to be used internally to libtransform.
#endif

#ifndef TRANSFORM_PRIVATE_H_INCLUDED
#define	TRANSFORM_PRIVATE_H_INCLUDED

#include "transform.h"
#include "transform_string.h"

#if defined(__GNUC__) && (__GNUC__ > 2 || \
			  (__GNUC__ == 2 && __GNUC_MINOR__ >= 5))
#define	__LA_DEAD	__attribute__((__noreturn__))
#else
#define	__LA_DEAD
#endif

#define	TRANSFORM_WRITE_MAGIC	(0xb0c5c0deU)
#define	TRANSFORM_READ_MAGIC	(0xdeb0c5U)
#define	TRANSFORM_WRITE_DISK_MAGIC (0xc001b0c5U)
#define	TRANSFORM_READ_DISK_MAGIC (0xbadb0c5U)

#define	TRANSFORM_STATE_NEW	1U
#define	TRANSFORM_STATE_HEADER	2U
#define	TRANSFORM_STATE_DATA	4U
#define	TRANSFORM_STATE_CLOSED	0x20U
#define	TRANSFORM_STATE_FATAL	0x8000U
#define	TRANSFORM_STATE_ANY	(0xFFFFU & ~TRANSFORM_STATE_FATAL)

struct transform_vtable {
	int	(*transform_close)(struct transform *);
	int	(*transform_free)(struct transform *);
	ssize_t	(*transform_write_data)(struct transform *,
	    const void *, size_t);
	ssize_t	(*transform_write_data_block)(struct transform *,
	    const void *, size_t, int64_t);

	int	(*transform_filter_count)(struct transform *);
	int64_t (*transform_filter_bytes)(struct transform *, int);
	int	(*transform_filter_code)(struct transform *, int);
	const char * (*transform_filter_name)(struct transform *, int);
};

struct transform {
	/*
	 * The magic/state values are used to sanity-check the
	 * client's usage.  If an API function is called at a
	 * ridiculous time, or the client passes us an invalid
	 * pointer, these values allow me to catch that.
	 */
	unsigned int	magic;
	unsigned int	state;

	/*
	 * Some public API functions depend on the "real" type of the
	 * transform object.
	 */
	struct transform_vtable *vtable;

	int		  transform_error_number;
	const char	 *error;
	struct transform_string	error_string;
};

/* Check magic value and state; return(TRANSFORM_FATAL) if it isn't valid. */
int	__transform_check_magic(struct transform *, unsigned int magic,
	    unsigned int state, const char *func);
#define	transform_check_magic(a, expected_magic, allowed_states, function_name) \
	do { \
		int magic_test = __transform_check_magic((a), (expected_magic), \
			(allowed_states), (function_name)); \
		if (magic_test == TRANSFORM_FATAL) \
			return TRANSFORM_FATAL; \
	} while (0)

void	__transform_errx(int retvalue, const char *msg) __LA_DEAD;

int	__transform_parse_options(const char *p, const char *fn,
	    int keysize, char *key, int valsize, char *val);

int	__transform_mktemp(const char *tmpdir);


#define	err_combine(a,b)	((a) < (b) ? (a) : (b))

#if defined(__BORLANDC__) || (defined(_MSC_VER) &&  _MSC_VER <= 1300)
# define	TRANSFORM_LITERAL_LL(x)	x##i64
# define	TRANSFORM_LITERAL_ULL(x)	x##ui64
#else
# define	TRANSFORM_LITERAL_LL(x)	x##ll
# define	TRANSFORM_LITERAL_ULL(x)	x##ull
#endif

#endif
