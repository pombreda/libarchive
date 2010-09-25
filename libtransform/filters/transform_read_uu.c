/*-
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

#include "transform_platform.h"
__FBSDID("$FreeBSD: head/lib/libtransform/transform_read_support_compression_uu.c 201248 2009-12-30 06:12:03Z kientzle $");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "transform.h"

struct uudecode {
	int64_t		 total;
	unsigned char	*in_buff;
#define IN_BUFF_SIZE	(1024)
	int		 in_cnt;
	size_t		 in_allocated;
	int		 state;
#define ST_FIND_HEAD	0
#define ST_READ_UU	1
#define ST_UUEND	2
#define ST_READ_BASE64	3
};


#define DEFAULT_BID_LOOKAHEAD 64 * 1024

static int	uudecode_bidder_bid(const void *, struct transform_read_filter *);
static int	uudecode_bidder_init(struct transform *, const void *);

static int	uudecode_filter_read(struct transform *, void *,
	struct transform_read_filter *upstream, const void **, size_t *);
static int	uudecode_filter_close(struct transform *, void *);

int
transform_read_add_uu(struct transform *_t)
{
	return (transform_read_bidder_add(_t, NULL, "uu", uudecode_bidder_bid,
		uudecode_bidder_init, NULL, NULL) == NULL ? TRANSFORM_FATAL : TRANSFORM_OK);
}

int
transform_autodetect_add_uu(struct transform_read_bidder *trb)
{
	return (transform_autodetect_add_bidder_create(trb, NULL, "uu",
		uudecode_bidder_bid, uudecode_bidder_init, NULL, NULL));
}

static const unsigned char ascii[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '\n', 0, 0, '\r', 0, 0, /* 00 - 0F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 10 - 1F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 20 - 2F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 30 - 3F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 40 - 4F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 50 - 5F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 60 - 6F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, /* 70 - 7F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 80 - 8F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 90 - 9F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* A0 - AF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* B0 - BF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* C0 - CF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* D0 - DF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* E0 - EF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* F0 - FF */
};

static const unsigned char uuchar[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 00 - 0F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 10 - 1F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 20 - 2F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 30 - 3F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 40 - 4F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 50 - 5F */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 60 - 6F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 70 - 7F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 80 - 8F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 90 - 9F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* A0 - AF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* B0 - BF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* C0 - CF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* D0 - DF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* E0 - EF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* F0 - FF */
};

static const unsigned char base64[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 00 - 0F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 10 - 1F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, /* 20 - 2F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, /* 30 - 3F */
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 40 - 4F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, /* 50 - 5F */
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 60 - 6F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, /* 70 - 7F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 80 - 8F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 90 - 9F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* A0 - AF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* B0 - BF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* C0 - CF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* D0 - DF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* E0 - EF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* F0 - FF */
};

static const int base64num[128] = {
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0, /* 00 - 0F */
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0, /* 10 - 1F */
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0, 62,  0,  0,  0, 63, /* 20 - 2F */
	52, 53, 54, 55, 56, 57, 58, 59,
	60, 61,  0,  0,  0,  0,  0,  0, /* 30 - 3F */
	 0,  0,  1,  2,  3,  4,  5,  6,
	 7,  8,  9, 10, 11, 12, 13, 14, /* 40 - 4F */
	15, 16, 17, 18, 19, 20, 21, 22,
	23, 24, 25,  0,  0,  0,  0,  0, /* 50 - 5F */
	 0, 26, 27, 28, 29, 30, 31, 32,
	33, 34, 35, 36, 37, 38, 39, 40, /* 60 - 6F */
	41, 42, 43, 44, 45, 46, 47, 48,
	49, 50, 51,  0,  0,  0,  0,  0, /* 70 - 7F */
};

static ssize_t
get_line(const unsigned char *b, ssize_t avail, ssize_t *nlsize)
{
	ssize_t len;

	len = 0;
	while (len < avail) {
		switch (ascii[*b]) {
		case 0:	/* Non-ascii character or control character. */
			if (nlsize != NULL)
				*nlsize = 0;
			return (-1);
		case '\r':
			if (avail-len > 1 && b[1] == '\n') {
				if (nlsize != NULL)
					*nlsize = 2;
				return (len+2);
			}
			/* FALL THROUGH */
		case '\n':
			if (nlsize != NULL)
				*nlsize = 1;
			return (len+1);
		case 1:
			b++;
			len++;
			break;
		}
	}
	if (nlsize != NULL)
		*nlsize = 0;
	return (avail);
}

static ssize_t
bid_get_line(struct transform_read_filter *filter,
    const unsigned char **b, ssize_t *avail, ssize_t *ravail, ssize_t *nl)
{
	ssize_t len;
	int quit;
	
	quit = 0;
	if (*avail == 0) {
		*nl = 0;
		len = 0;
	} else
		len = get_line(*b, *avail, nl);
	/*
	 * Read bytes more while it does not reach the end of line.
	 */
	while (*nl == 0 && len == *avail && !quit) {
		ssize_t diff = *ravail - *avail;

		*b = transform_read_filter_ahead(filter, 160 + *ravail, avail);
		if (*b == NULL) {
			if (*ravail >= *avail)
				return (0);
			/* Reading bytes reaches the end of file. */
			*b = transform_read_filter_ahead(filter, *avail, avail);
			quit = 1;
		}
		*ravail = *avail;
		*b += diff;
		*avail -= diff;
		len = get_line(*b, *avail, nl);
	}
	return (len);
}

#define UUDECODE(c) (((c) - 0x20) & 0x3f)

static int
uudecode_bidder_bid(const void *_bidder_data,
    struct transform_read_filter *filter)
{
	const unsigned char *b;
	ssize_t avail, ravail;
	ssize_t len, nl;
	int l;
	int firstline;

	b = transform_read_filter_ahead(filter, DEFAULT_BID_LOOKAHEAD, &avail);
	if (!b) {
		/* EOF was hit; grab what's available and just check that */
		b = transform_read_filter_ahead(filter, 1, &avail);
		if (!b) {
			return (0);
		}
	}

	firstline = 20;
	ravail = avail;
	for (;;) {
		len = bid_get_line(filter, &b, &avail, &ravail, &nl);
		if (len < 0 || nl == 0)
			return (0);/* Binary data. */
		if (len - nl >= 11 && memcmp(b, "begin ", 6) == 0)
			l = 6;
		else if (len -nl >= 18 && memcmp(b, "begin-base64 ", 13) == 0)
			l = 13;
		else
			l = 0;

		if (l > 0 && (b[l] < '0' || b[l] > '7' ||
		    b[l+1] < '0' || b[l+1] > '7' ||
		    b[l+2] < '0' || b[l+2] > '7' || b[l+3] != ' '))
			l = 0;

		b += len;
		avail -= len;
		if (l)
			break;
		firstline = 0;
	}
	if (!avail)
		return (0);
	len = bid_get_line(filter, &b, &avail, &ravail, &nl);
	if (len < 0 || nl == 0)
		return (0);/* There are non-ascii characters. */
	avail -= len;

	if (l == 6) {
		if (!uuchar[*b])
			return (0);
		/* Get a length of decoded bytes. */
		l = UUDECODE(*b++); len--;
		if (l > 45)
			/* Normally, maximum length is 45(character 'M'). */
			return (0);
		while (l && len-nl > 0) {
			if (l > 0) {
				if (!uuchar[*b++])
					return (0);
				if (!uuchar[*b++])
					return (0);
				len -= 2;
				--l;
			}
			if (l > 0) {
				if (!uuchar[*b++])
					return (0);
				--len;
				--l;
			}
			if (l > 0) {
				if (!uuchar[*b++])
					return (0);
				--len;
				--l;
			}
		}
		if (len-nl < 0)
			return (0);
		if (len-nl == 1 &&
		    (uuchar[*b] ||		 /* Check sum. */
		     (*b >= 'a' && *b <= 'z'))) {/* Padding data(MINIX). */
			++b;
			--len;
		}
		b += nl;
		if (avail && uuchar[*b])
			return (firstline+30);
	}
	if (l == 13) {
		while (len-nl > 0) {
			if (!base64[*b++])
				return (0);
			--len;
		}
		b += nl;

		if (avail >= 5 && memcmp(b, "====\n", 5) == 0)
			return (firstline+40);
		if (avail >= 6 && memcmp(b, "====\r\n", 6) == 0)
			return (firstline+40);
		if (avail > 0 && base64[*b])
			return (firstline+30);
	}

	return (0);
}

static int
uudecode_bidder_init(struct transform *transform, const void *bidder_data)
{
	struct uudecode   *uudecode;
	void *in_buff;
	int ret;

	uudecode = (struct uudecode *)calloc(sizeof(*uudecode), 1);
	in_buff = malloc(IN_BUFF_SIZE);
	if (uudecode == NULL ||  in_buff == NULL) {
		transform_set_error(transform, ENOMEM,
		    "Can't allocate data for uudecode");
		free(uudecode);
		free(in_buff);
		return (TRANSFORM_FATAL);
	}

	uudecode->in_buff = in_buff;
	uudecode->in_cnt = 0;
	uudecode->in_allocated = IN_BUFF_SIZE;
	uudecode->state = ST_FIND_HEAD;

	ret = transform_read_add_new_filter(transform, (void *)uudecode,
		"uu", TRANSFORM_FILTER_UU,
		uudecode_filter_read, NULL, uudecode_filter_close, NULL, 0);

	if (TRANSFORM_OK != ret) {
		uudecode_filter_close(transform, (void *)uudecode);
	}
	return (ret);
}

static int
ensure_in_buff_size(struct transform *transform,
    struct uudecode *uudecode, size_t size)
{

	if (size > uudecode->in_allocated) {
		unsigned char *ptr;
		size_t newsize;

		newsize = uudecode->in_allocated << 1;
		ptr = malloc(newsize);
		if (ptr == NULL ||
		    newsize < uudecode->in_allocated) {
			free(ptr);
			transform_set_error(transform,
			    ENOMEM,
    			    "Can't allocate data for uudecode");
			return (TRANSFORM_FATAL);
		}
		if (uudecode->in_cnt)
			memmove(ptr, uudecode->in_buff,
			    uudecode->in_cnt);
		free(uudecode->in_buff);
		uudecode->in_buff = ptr;
		uudecode->in_allocated = newsize;
	}
	return (TRANSFORM_OK);
}

static int
uudecode_filter_read(struct transform *transform, void *_data,
	struct transform_read_filter *upstream, const void **buff,
	size_t *bytes_read)
{
	struct uudecode *uudecode = (struct uudecode *)_data;
	const unsigned char *b, *d;
	unsigned char *out;
	ssize_t avail_in, ravail;
	ssize_t used;
	ssize_t total;
	ssize_t len, llen, nl;

read_more:
	d = transform_read_filter_ahead(upstream, 1, &avail_in);
	if (d == NULL && avail_in < 0)
		return (TRANSFORM_FATAL);
	/* Quiet a code analyzer; make sure avail_in must be zero
	 * when d is NULL. */
	if (d == NULL)
		avail_in = 0;
	used = 0;
	total = 0;
	out = (void *)*buff;
	ravail = avail_in;
	if (uudecode->in_cnt) {
		/*
		 * If there is remaining data which is saved by
		 * previous calling, use it first.
		 */
		if (ensure_in_buff_size(transform, uudecode,
		    avail_in + uudecode->in_cnt) != TRANSFORM_OK)
			return (TRANSFORM_FATAL);
		memcpy(uudecode->in_buff + uudecode->in_cnt,
		    d, avail_in);
		d = uudecode->in_buff;
		avail_in += uudecode->in_cnt;
		uudecode->in_cnt = 0;
	}
	for (;used < avail_in; d += llen, used += llen) {
		int l, body;

		b = d;
		len = get_line(b, avail_in - used, &nl);
		if (len < 0) {
			/* Non-ascii character is found. */
			transform_set_error(transform,
			    TRANSFORM_ERRNO_MISC,
			    "Insufficient compressed data");
			return (TRANSFORM_FATAL);
		}
		llen = len;
		if (nl == 0) {
			/*
			 * Save remaining data which does not contain
			 * NL('\n','\r').
			 */
			if (ensure_in_buff_size(transform, uudecode, len)
			    != TRANSFORM_OK)
				return (TRANSFORM_FATAL);
			if (uudecode->in_buff != b)
				memmove(uudecode->in_buff, b, len);
			uudecode->in_cnt = len;
			if (total == 0) {
				/* Do not return 0; it means end-of-file.
				 * We should try to read bytes more. */
				transform_read_filter_consume(
				    upstream, ravail);
				goto read_more;
			}
			break;
		}
		if (total + len * 2 > *bytes_read)
			break;
		switch (uudecode->state) {
		default:
		case ST_FIND_HEAD:
			if (len - nl > 13 && memcmp(b, "begin ", 6) == 0)
				l = 6;
			else if (len - nl > 18 &&
			    memcmp(b, "begin-base64 ", 13) == 0)
				l = 13;
			else
				l = 0;
			if (l != 0 && b[l] >= '0' && b[l] <= '7' &&
			    b[l+1] >= '0' && b[l+1] <= '7' &&
			    b[l+2] >= '0' && b[l+2] <= '7' && b[l+3] == ' ') {
				if (l == 6)
					uudecode->state = ST_READ_UU;
				else
					uudecode->state = ST_READ_BASE64;
			}
			break;
		case ST_READ_UU:
			body = len - nl;
			if (!uuchar[*b] || body <= 0) {
				transform_set_error(transform,
				    TRANSFORM_ERRNO_MISC,
				    "Insufficient compressed data");
				return (TRANSFORM_FATAL);
			}
			/* Get length of undecoded bytes of curent line. */
			l = UUDECODE(*b++);
			body--;
			if (l > body) {
				transform_set_error(transform,
				    TRANSFORM_ERRNO_MISC,
				    "Insufficient compressed data");
				return (TRANSFORM_FATAL);
			}
			if (l == 0) {
				uudecode->state = ST_UUEND;
				break;
			}
			while (l > 0) {
				int n = 0;

				if (l > 0) {
					if (!uuchar[b[0]] || !uuchar[b[1]])
						break;
					n = UUDECODE(*b++) << 18;
					n |= UUDECODE(*b++) << 12;
					*out++ = n >> 16; total++;
					--l;
				}
				if (l > 0) {
					if (!uuchar[b[0]])
						break;
					n |= UUDECODE(*b++) << 6;
					*out++ = (n >> 8) & 0xFF; total++;
					--l;
				}
				if (l > 0) {
					if (!uuchar[b[0]])
						break;
					n |= UUDECODE(*b++);
					*out++ = n & 0xFF; total++;
					--l;
				}
			}
			if (l) {
				transform_set_error(transform,
				    TRANSFORM_ERRNO_MISC,
				    "Insufficient compressed data");
				return (TRANSFORM_FATAL);
			}
			break;
		case ST_UUEND:
			if (len - nl == 3 && memcmp(b, "end ", 3) == 0)
				uudecode->state = ST_FIND_HEAD;
			else {
				transform_set_error(transform,
				    TRANSFORM_ERRNO_MISC,
				    "Insufficient compressed data");
				return (TRANSFORM_FATAL);
			}
			break;
		case ST_READ_BASE64:
			l = len - nl;
			if (l >= 3 && b[0] == '=' && b[1] == '=' &&
			    b[2] == '=') {
				uudecode->state = ST_FIND_HEAD;
				break;
			}
			while (l > 0) {
				int n = 0;

				if (l > 0) {
					if (!base64[b[0]] || !base64[b[1]])
						break;
					n = base64num[*b++] << 18;
					n |= base64num[*b++] << 12;
					*out++ = n >> 16; total++;
					l -= 2;
				}
				if (l > 0) {
					if (*b == '=')
						break;
					if (!base64[*b])
						break;
					n |= base64num[*b++] << 6;
					*out++ = (n >> 8) & 0xFF; total++;
					--l;
				}
				if (l > 0) {
					if (*b == '=')
						break;
					if (!base64[*b])
						break;
					n |= base64num[*b++];
					*out++ = n & 0xFF; total++;
					--l;
				}
			}
			if (l && *b != '=') {
				transform_set_error(transform,
				    TRANSFORM_ERRNO_MISC,
				    "Insufficient compressed data");
				return (TRANSFORM_FATAL);
			}
			break;
		}
	}

	transform_read_filter_consume(upstream, ravail);

	uudecode->total += total;
	*bytes_read = total;
	return (total == 0 ? TRANSFORM_EOF : TRANSFORM_OK);
}

static int
uudecode_filter_close(struct transform *transform, void *_data)
{
	struct uudecode *uudecode = (struct uudecode *)_data;

	(void)transform; /* UNUSED */

	free(uudecode->in_buff);
	free(uudecode);

	return (TRANSFORM_OK);
}
