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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <string.h>

#include "transform.h"
#include "transform_endian.h"

struct rpm {
	int64_t		 total_in;
	size_t		 hpos;
	size_t		 hlen;
	unsigned char	 header[16];
	enum {
		ST_LEAD,	/* Skipping 'Lead' section. */
		ST_HEADER,	/* Reading 'Header' section;
				 * first 16 bytes. */
		ST_HEADER_DATA,	/* Skipping 'Header' section. */
		ST_PADDING,	/* Skipping padding data after the
				 * 'Header' section. */
		ST_TRANSFORM	/* Reading 'Archive' section. */
	}		 state;
	int		 first_header;
};
#define RPM_LEAD_SIZE	96	/* Size of 'Lead' section. */

static int	rpm_bidder_bid(const void *, struct transform_read_filter *upstream);
static int	rpm_bidder_init(struct transform *, const void *);

static ssize_t	rpm_filter_read(struct transform *, void *,
	struct transform_read_filter *upstream, const void **);
static int	rpm_filter_close(struct transform *, void *);

int
transform_read_add_rpm(struct transform *_t)
{
	return (transform_read_bidder_add(_t, NULL, "rpm", rpm_bidder_bid,
		rpm_bidder_init, NULL, NULL) == NULL ? TRANSFORM_FATAL : TRANSFORM_OK);
}

int
transform_autodetect_add_rpm(struct transform_read_bidder *trb)
{
	return (transform_autodetect_add_bidder_create(trb, NULL, "rpm", rpm_bidder_bid,
		rpm_bidder_init, NULL, NULL));
}

static int
rpm_bidder_bid(const void *_data, struct transform_read_filter *upstream)
{
	const unsigned char *b;
	ssize_t avail;
	int bits_checked;

	b = transform_read_filter_ahead(upstream, 8, &avail);
	if (b == NULL)
		return (0);

	bits_checked = 0;
	/*
	 * Verify Header Magic Bytes : 0xed 0xab 0xee 0xdb
	 */
	if (b[0] != 0xed)
		return (0);
	bits_checked += 8;
	if (b[1] != 0xab)
		return (0);
	bits_checked += 8;
	if (b[2] != 0xee)
		return (0);
	bits_checked += 8;
	if (b[3] != 0xdb)
		return (0);
	bits_checked += 8;
	/*
	 * Check major version.
	 */
	if (b[4] != 3 && b[4] != 4)
		return (0);
	bits_checked += 8;
	/*
	 * Check package type; binary or source.
	 */
	if (b[6] != 0)
		return (0);
	bits_checked += 8;
	if (b[7] != 0 && b[7] != 1)
		return (0);
	bits_checked += 8;

	return (bits_checked);
}

static int
rpm_bidder_init(struct transform *transform, const void *bidder_data)
{
	struct rpm *rpm;
	int ret;

	rpm = (struct rpm *)calloc(sizeof(*rpm), 1);
	if (rpm == NULL) {
		transform_set_error(transform, ENOMEM,
		    "Can't allocate data for rpm");
		return (TRANSFORM_FATAL);
	}
	rpm->state = ST_LEAD;

	ret = transform_read_filter_add(transform, (void *)rpm,
		"rpm", TRANSFORM_FILTER_RPM,
		rpm_filter_read, NULL, rpm_filter_close, NULL);
	if (TRANSFORM_OK != ret) {
		rpm_filter_close(transform, rpm);
	}

	return (ret);
}

static ssize_t
rpm_filter_read(struct transform *transform, void *_data,
	struct transform_read_filter *upstream, const void **buff)
{
	struct rpm *rpm = (struct rpm *)_data;
	const unsigned char *b;
	ssize_t avail_in, total;
	size_t used, n;
	uint32_t section;
	uint32_t bytes;

	*buff = NULL;
	total = avail_in = 0;
	b = NULL;
	used = 0;
	do {
		if (b == NULL) {
			b = transform_read_filter_ahead(upstream, 1,
			    &avail_in);
			if (b == NULL) {
				if (avail_in < 0)
					return (TRANSFORM_FATAL);
				else
					break;
			}
		}

		switch (rpm->state) {
		case ST_LEAD:
			if (rpm->total_in + avail_in < RPM_LEAD_SIZE)
				used += avail_in;
			else {
				n = RPM_LEAD_SIZE - rpm->total_in;
				used += n;
				b += n;
				rpm->state = ST_HEADER;
				rpm->hpos = 0;
				rpm->hlen = 0;
				rpm->first_header = 1;
			}
			break;
		case ST_HEADER:
			n = 16 - rpm->hpos;
			if (n > avail_in - used)
				n = avail_in - used;
			memcpy(rpm->header+rpm->hpos, b, n);
			b += n;
			used += n;
			rpm->hpos += n;

			if (rpm->hpos == 16) {
				if (rpm->header[0] != 0x8e ||
				    rpm->header[1] != 0xad ||
				    rpm->header[2] != 0xe8 ||
				    rpm->header[3] != 0x01) {
					if (rpm->first_header) {
						transform_set_error(
						    transform,
						    TRANSFORM_ERRNO_FILE_FORMAT,
						    "Unrecoginized rpm header");
						return (TRANSFORM_FATAL);
					}
					rpm->state = ST_TRANSFORM;
					*buff = rpm->header;
					total = rpm->hpos;
					break;
				}
				/* Calculate 'Header' length. */
				section = transform_be32dec(rpm->header+8);
				bytes = transform_be32dec(rpm->header+12);
				rpm->hlen = 16 + section * 16 + bytes;
				rpm->state = ST_HEADER_DATA;
				rpm->first_header = 0;
			}
			break;
		case ST_HEADER_DATA:
			n = rpm->hlen - rpm->hpos;
			if (n > avail_in - used)
				n = avail_in - used;
			b += n;
			used += n;
			rpm->hpos += n;
			if (rpm->hpos == rpm->hlen)
				rpm->state = ST_PADDING;
			break;
		case ST_PADDING:
			while (used < (size_t)avail_in) {
				if (*b != 0) {
					/* Read next header. */
					rpm->state = ST_HEADER;
					rpm->hpos = 0;
					rpm->hlen = 0;
					break;
				}
				b++;
				used++;
			}
			break;
		case ST_TRANSFORM:
			*buff = b;
			total = avail_in;
			used = avail_in;
			break;
		}
		if (used == (size_t)avail_in) {
			rpm->total_in += used;
			transform_read_filter_consume(upstream, used);
			b = NULL;
			used = 0;
		}
	} while (total == 0 && avail_in > 0);

	if (used > 0 && b != NULL) {
		rpm->total_in += used;
		transform_read_filter_consume(upstream, used);
	}
	return (total);
}

static int
rpm_filter_close(struct transform *transform, void *_data)
{
	struct rpm *data = (struct rpm *)_data;

	(void)transform; /* UNUSED */

	free(data);

	return (TRANSFORM_OK);
}

