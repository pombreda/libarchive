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
__FBSDID("$FreeBSD: head/lib/libtransform/transform_check_magic.c 201089 2009-12-28 02:20:23Z kientzle $");

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#if defined(_WIN32) && !defined(__CYGWIN__)
#include <windows.h>
#include <winbase.h>
#endif

#include "transform_private.h"

static void
errmsg(const char *m)
{
	size_t s = strlen(m);
	ssize_t written;

	while (s > 0) {
		written = write(2, m, strlen(m));
		if (written <= 0)
			return;
		m += written;
		s -= written;
	}
}

static void
diediedie(void)
{
#if defined(_WIN32) && !defined(__CYGWIN__) && defined(_DEBUG)
	/* Cause a breakpoint exception  */
	DebugBreak();
#endif
	abort();        /* Terminate the program abnormally. */
}

static const char *
state_name(unsigned s)
{
	switch (s) {
	case TRANSFORM_STATE_NEW:		return ("new");
	case TRANSFORM_STATE_DATA:	return ("data");
	case TRANSFORM_STATE_CLOSED:	return ("closed");
	case TRANSFORM_STATE_FATAL:	return ("fatal");
	default:			return ("??");
	}
}


static char *
write_all_states(char *buff, unsigned int states)
{
	unsigned int lowbit;

	buff[0] = '\0';

	/* A trick for computing the lowest set bit. */
	while ((lowbit = states & (1 + ~states)) != 0) {
		states &= ~lowbit;		/* Clear the low bit. */
		strcat(buff, state_name(lowbit));
		if (states != 0)
			strcat(buff, "/");
	}
	return buff;
}

/*
 * Check magic value and current state.
 *   Magic value mismatches are fatal and result in calls to abort().
 *   State mismatches return TRANSFORM_FATAL.
 *   Otherwise, returns TRANSFORM_OK.
 *
 * This is designed to catch serious programming errors that violate
 * the libtransform API.
 */
int
__transform_check_state(struct marker *m, unsigned int magic,
    unsigned int state, const char *function)
{
	if (m->magic != magic) {
		errmsg("PROGRAMMER ERROR: Function ");
		errmsg(function);
		errmsg(" invoked with invalid transform handle.\n");
		diediedie();
	}

	if ((m->state & state) != 0) {
		return (TRANSFORM_OK);
	}
	m->state = TRANSFORM_STATE_FATAL;
	return (TRANSFORM_FATAL);
}

int
__transform_check_magic(struct transform *t, unsigned int magic,
	unsigned int state, const char *function)
{
	struct marker *m = &(t->marker);
	char states1[64];
	char states2[64];
	int raw_state = m->state;
	
	if(TRANSFORM_OK == __transform_check_state(m, magic, state, function)) {
		return (TRANSFORM_OK);
	}

	/* If we're already FATAL, don't overwrite the error. */
	if (raw_state != TRANSFORM_STATE_FATAL) {
		transform_set_error(t, -1,
		    "INTERNAL ERROR: Function '%s' invoked with"
	    	" transform structure in state '%s',"
		    " should be in state '%s'",
		    function,
	    	write_all_states(states1, raw_state),
		    write_all_states(states2, state));
	}
	return (TRANSFORM_FATAL);
}
