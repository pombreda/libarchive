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
 * $FreeBSD: head/lib/libtransform/transform_read_private.h 201088 2009-12-28 02:18:55Z kientzle $
 */

#ifndef __LIBTRANSFORM_BUILD
#error This header is only to be used internally to libtransform.
#endif

#ifndef TRANSFORM_READ_PRIVATE_H_INCLUDED
#define	TRANSFORM_READ_PRIVATE_H_INCLUDED

#include "transform.h"
#include "transform_string.h"
#include "transform_private.h"

struct transform_read;
struct transform_read_bidder;
struct transform_read_filter;

int transform_read_open2(struct transform *_a, void *client_data,
    transform_read_callback *client_reader,
    transform_skip_callback *client_skipper,
    transform_close_callback *client_closer,
    transform_read_filter_visit_fds_callback *,
    int64_t flags);

/*
 * How bidding works for filters:
 *   * The bid manager initializes the client-provided reader as the
 *     first filter.
 *   * It invokes the bidder for each registered filter with the
 *     current head filter.
 *   * The bidders can use transform_read_filter_ahead() to peek ahead
 *     at the incoming data to compose their bids.
 *   * The bid manager creates a new filter structure for the winning
 *     bidder and gives the winning bidder a chance to initialize it.
 *   * The new filter becomes the new top filter and we repeat the
 *     process.
 * This ends only when no bidder provides a non-zero bid.  Then
 * we perform a similar dance with the registered format handlers.
 */
struct transform_read_bidder {
	struct marker marker;

	/* Configuration data for the bidder. */
	void *data;

	const char *name;

	/* Taste the upstream filter to see if we handle this. */
	transform_read_bidder_bid_callback *bid;
	/* create a new filter. */
	transform_read_bidder_create_filter_callback *create_filter;

	/* Set an option for the filter bidder. */
	transform_read_bidder_set_option_callback *set_option;

	/* Release the bidder's configuration data. */
	transform_read_bidder_free_callback *free;

	struct transform_read_bidder *next;
};

/*
 * This structure is allocated within the transform_read core
 * and initialized by transform_read and the init() method of the
 * corresponding bidder above.
 */
struct transform_read_filter {

	struct marker marker;

	/* these are callback defined methods */
	transform_read_filter_read_callback *read;
	transform_read_filter_skip_callback *skip;
	transform_read_filter_close_callback *close;
	transform_read_filter_visit_fds_callback *visit_fds;

	struct transform_read_filter *upstream; /* Who I read from. */
	
	const char	*name;
	int		 code;

	/* My private data. */
	void *data;

	int64_t flags;

	/* everything following is managed by libtransform */

	struct transform_read *transform; /* Associated transform. */

	int64_t bytes_consumed;

	/* Used by reblocking logic.
	 * for example, if the invoking code requests a lookahead 2x the
	 * blocking, the resultant char * that the data is accrued in
	 * is buffer.
	 */

	struct transform_buffer resizing;

	size_t buff_size_preferred;
	void   *managed_buffer;
	size_t managed_size;
	/* 
	 * this is our upstream exposed buffers.
	 */
	struct transform_buffer_borrowed client;

	/* filter state info */
	char		 end_of_file;
	char		 fatal;
};

/*
 * The client looks a lot like a filter, so we just wrap it here.
 *
 * TODO: Make transform_read_filter and transform_read_client identical so
 * that users of the library can easily register their own
 * transformation filters.  This will probably break the API/ABI and
 * so should be deferred at least until libtransform 3.0.
 */
struct transform_read_client {
	transform_read_callback	*reader;
	transform_skip_callback	*skipper;
	transform_close_callback	*closer;
};

struct transform_read {
	struct transform	transform;

	/* Callbacks to open/read/write/close client transform stream. */
	struct transform_read_client client;

	/* Registered filter bidders. */
	struct transform_read_bidder *bidders;

	/* Last filter in chain */
	struct transform_read_filter *filter;

};

int __transform_read_program(struct transform *, const char *, const char *, int);
#endif
