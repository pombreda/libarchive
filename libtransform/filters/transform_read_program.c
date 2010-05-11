/*-
 * Copyright (c) 2007 Joerg Sonnenberger
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
__FBSDID("$FreeBSD: head/lib/libtransform/transform_read_support_compression_program.c 201112 2009-12-28 06:59:35Z kientzle $");

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#ifdef HAVE_ERRNO_H
#  include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#ifdef HAVE_LIMITS_H
#  include <limits.h>
#endif
#ifdef HAVE_SIGNAL_H
#  include <signal.h>
#endif
#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "transform.h"
#include "transform_private.h"
#include "transform_read_private.h"

int
transform_read_support_compression_program(struct transform *a, const char *cmd)
{
	return (transform_read_support_compression_program_signature(a, cmd, NULL, 0));
}


/* This capability is only available on POSIX systems. */
#if (!defined(HAVE_PIPE) || !defined(HAVE_FCNTL) || \
    !(defined(HAVE_FORK) || defined(HAVE_VFORK))) && (!defined(_WIN32) || defined(__CYGWIN__))

/*
 * On non-Posix systems, allow the program to build, but choke if
 * this function is actually invoked.
 */
int
transform_read_support_compression_program_signature(struct transform *_a,
    const char *cmd, void *signature, size_t signature_len)
{
	(void)_a; /* UNUSED */
	(void)cmd; /* UNUSED */
	(void)signature; /* UNUSED */
	(void)signature_len; /* UNUSED */

	transform_set_error(_a, -1,
	    "External compression programs not supported on this platform");
	return (TRANSFORM_FATAL);
}

int
__transform_read_program(struct transform_read_filter *self, const char *cmd)
{
	(void)self; /* UNUSED */
	(void)cmd; /* UNUSED */

	transform_set_error(&self->transform->transform, -1,
	    "External compression programs not supported on this platform");
	return (TRANSFORM_FATAL);
}

#else

#include "filter_fork.h"

/*
 * The bidder object stores the command and the signature to watch for.
 * The 'inhibit' entry here is used to ensure that unchecked filters never
 * bid twice in the same pipeline.
 */
struct program_bidder {
	char *cmd;
	void *signature;
	size_t signature_len;
	int inhibit;
};

static int	program_bidder_bid(struct transform_read_filter_bidder *,
		    struct transform_read_filter *upstream);
static int	program_bidder_init(struct transform_read_filter *);
static int	program_bidder_free(struct transform_read_filter_bidder *);

/*
 * The actual filter needs to track input and output data.
 */
struct program_filter {
	char		*description;
	pid_t		 child;
	int		 exit_status;
	int		 waitpid_return;
	int		 child_stdin, child_stdout;

	char		*out_buf;
	size_t		 out_buf_len;
};

static ssize_t	program_filter_read(struct transform_read_filter *,
		    const void **);
static int	program_filter_close(struct transform_read_filter *);

int
transform_read_support_compression_program_signature(struct transform *_a,
    const char *cmd, const void *signature, size_t signature_len)
{
	struct transform_read *a = (struct transform_read *)_a;
	struct transform_read_filter_bidder *bidder;
	struct program_bidder *state;

	/*
	 * Get a bidder object from the read core.
	 */
	bidder = __transform_read_get_bidder(a);
	if (bidder == NULL)
		return (TRANSFORM_FATAL);

	/*
	 * Allocate our private state.
	 */
	state = (struct program_bidder *)calloc(sizeof (*state), 1);
	if (state == NULL)
		return (TRANSFORM_FATAL);
	state->cmd = strdup(cmd);
	if (signature != NULL && signature_len > 0) {
		state->signature_len = signature_len;
		state->signature = malloc(signature_len);
		memcpy(state->signature, signature, signature_len);
	}

	/*
	 * Fill in the bidder object.
	 */
	bidder->data = state;
	bidder->bid = program_bidder_bid;
	bidder->init = program_bidder_init;
	bidder->options = NULL;
	bidder->free = program_bidder_free;
	return (TRANSFORM_OK);
}

static int
program_bidder_free(struct transform_read_filter_bidder *self)
{
	struct program_bidder *state = (struct program_bidder *)self->data;
	free(state->cmd);
	free(state->signature);
	free(self->data);
	return (TRANSFORM_OK);
}

/*
 * If we do have a signature, bid only if that matches.
 *
 * If there's no signature, we bid INT_MAX the first time
 * we're called, then never bid again.
 */
static int
program_bidder_bid(struct transform_read_filter_bidder *self,
    struct transform_read_filter *upstream)
{
	struct program_bidder *state = self->data;
	const char *p;

	/* If we have a signature, use that to match. */
	if (state->signature_len > 0) {
		p = __transform_read_filter_ahead(upstream,
		    state->signature_len, NULL);
		if (p == NULL)
			return (0);
		/* No match, so don't bid. */
		if (memcmp(p, state->signature, state->signature_len) != 0)
			return (0);
		return ((int)state->signature_len * 8);
	}

	/* Otherwise, bid once and then never bid again. */
	if (state->inhibit)
		return (0);
	state->inhibit = 1;
	return (INT_MAX);
}

/*
 * Shut down the child, return TRANSFORM_OK if it exited normally.
 *
 * Note that the return value is sticky; if we're called again,
 * we won't reap the child again, but we will return the same status
 * (including error message if the child came to a bad end).
 */
static int
child_stop(struct transform_read_filter *self, struct program_filter *state)
{
	/* Close our side of the I/O with the child. */
	if (state->child_stdin != -1) {
		close(state->child_stdin);
		state->child_stdin = -1;
	}
	if (state->child_stdout != -1) {
		close(state->child_stdout);
		state->child_stdout = -1;
	}

	if (state->child != 0) {
		/* Reap the child. */
		do {
			state->waitpid_return
			    = waitpid(state->child, &state->exit_status, 0);
		} while (state->waitpid_return == -1 && errno == EINTR);
		state->child = 0;
	}

	if (state->waitpid_return < 0) {
		/* waitpid() failed?  This is ugly. */
		transform_set_error(&self->transform->transform, TRANSFORM_ERRNO_MISC,
		    "Child process exited badly");
		return (TRANSFORM_WARN);
	}

#if !defined(_WIN32) || defined(__CYGWIN__)
	if (WIFSIGNALED(state->exit_status)) {
#ifdef SIGPIPE
		/* If the child died because we stopped reading before
		 * it was done, that's okay.  Some transform formats
		 * have padding at the end that we routinely ignore. */
		/* The alternative to this would be to add a step
		 * before close(child_stdout) above to read from the
		 * child until the child has no more to write. */
		if (WTERMSIG(state->exit_status) == SIGPIPE)
			return (TRANSFORM_OK);
#endif
		transform_set_error(&self->transform->transform, TRANSFORM_ERRNO_MISC,
		    "Child process exited with signal %d",
		    WTERMSIG(state->exit_status));
		return (TRANSFORM_WARN);
	}
#endif /* !_WIN32 || __CYGWIN__ */

	if (WIFEXITED(state->exit_status)) {
		if (WEXITSTATUS(state->exit_status) == 0)
			return (TRANSFORM_OK);

		transform_set_error(&self->transform->transform,
		    TRANSFORM_ERRNO_MISC,
		    "Child process exited with status %d",
		    WEXITSTATUS(state->exit_status));
		return (TRANSFORM_WARN);
	}

	return (TRANSFORM_WARN);
}

/*
 * Use select() to decide whether the child is ready for read or write.
 */
static ssize_t
child_read(struct transform_read_filter *self, char *buf, size_t buf_len)
{
	struct program_filter *state = self->data;
	ssize_t ret, requested, avail;
	const char *p;

	requested = buf_len > SSIZE_MAX ? SSIZE_MAX : buf_len;

	for (;;) {
		do {
			ret = read(state->child_stdout, buf, requested);
		} while (ret == -1 && errno == EINTR);

		if (ret > 0)
			return (ret);
		if (ret == 0 || (ret == -1 && errno == EPIPE))
			/* Child has closed its output; reap the child
			 * and return the status. */
			return (child_stop(self, state));
		if (ret == -1 && errno != EAGAIN)
			return (-1);

		if (state->child_stdin == -1) {
			/* Block until child has some I/O ready. */
			__transform_check_child(state->child_stdin,
			    state->child_stdout);
			continue;
		}

		/* Get some more data from upstream. */
		p = __transform_read_filter_ahead(self->upstream, 1, &avail);
		if (p == NULL) {
			close(state->child_stdin);
			state->child_stdin = -1;
			fcntl(state->child_stdout, F_SETFL, 0);
			if (avail < 0)
				return (avail);
			continue;
		}

		do {
			ret = write(state->child_stdin, p, avail);
		} while (ret == -1 && errno == EINTR);

		if (ret > 0) {
			/* Consume whatever we managed to write. */
			__transform_read_filter_consume(self->upstream, ret);
		} else if (ret == -1 && errno == EAGAIN) {
			/* Block until child has some I/O ready. */
			__transform_check_child(state->child_stdin,
			    state->child_stdout);
		} else {
			/* Write failed. */
			close(state->child_stdin);
			state->child_stdin = -1;
			fcntl(state->child_stdout, F_SETFL, 0);
			/* If it was a bad error, we're done; otherwise
			 * it was EPIPE or EOF, and we can still read
			 * from the child. */
			if (ret == -1 && errno != EPIPE)
				return (-1);
		}
	}
}

int
__transform_read_program(struct transform_read_filter *self, const char *cmd)
{
	struct program_filter	*state;
	static const size_t out_buf_len = 65536;
	char *out_buf;
	char *description;
	const char *prefix = "Program: ";

	state = (struct program_filter *)calloc(1, sizeof(*state));
	out_buf = (char *)malloc(out_buf_len);
	description = (char *)malloc(strlen(prefix) + strlen(cmd) + 1);
	if (state == NULL || out_buf == NULL || description == NULL) {
		transform_set_error(&self->transform->transform, ENOMEM,
		    "Can't allocate input data");
		free(state);
		free(out_buf);
		free(description);
		return (TRANSFORM_FATAL);
	}

	self->code = TRANSFORM_FILTER_PROGRAM;
	state->description = description;
	strcpy(state->description, prefix);
	strcat(state->description, cmd);
	self->name = state->description;

	state->out_buf = out_buf;
	state->out_buf_len = out_buf_len;

	if ((state->child = __transform_create_child(cmd,
		 &state->child_stdin, &state->child_stdout)) == -1) {
		free(state->out_buf);
		free(state);
		transform_set_error(&self->transform->transform, EINVAL,
		    "Can't initialise filter");
		return (TRANSFORM_FATAL);
	}

	self->data = state;
	self->read = program_filter_read;
	self->skip = NULL;
	self->close = program_filter_close;

	/* XXX Check that we can read at least one byte? */
	return (TRANSFORM_OK);
}

static int
program_bidder_init(struct transform_read_filter *self)
{
	struct program_bidder   *bidder_state;

	bidder_state = (struct program_bidder *)self->bidder->data;
	return (__transform_read_program(self, bidder_state->cmd));
}

static ssize_t
program_filter_read(struct transform_read_filter *self, const void **buff)
{
	struct program_filter *state;
	ssize_t bytes;
	size_t total;
	char *p;

	state = (struct program_filter *)self->data;

	total = 0;
	p = state->out_buf;
	while (state->child_stdout != -1 && total < state->out_buf_len) {
		bytes = child_read(self, p, state->out_buf_len - total);
		if (bytes < 0)
			/* No recovery is possible if we can no longer
			 * read from the child. */
			return (TRANSFORM_FATAL);
		if (bytes == 0)
			/* We got EOF from the child. */
			break;
		total += bytes;
		p += bytes;
	}

	*buff = state->out_buf;
	return (total);
}

static int
program_filter_close(struct transform_read_filter *self)
{
	struct program_filter	*state;
	int e;

	state = (struct program_filter *)self->data;
	e = child_stop(self, state);

	/* Release our private data. */
	free(state->out_buf);
	free(state->description);
	free(state);

	return (e);
}

#endif /* !defined(HAVE_PIPE) || !defined(HAVE_VFORK) || !defined(HAVE_FCNTL) */