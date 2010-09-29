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
 */

#include "transform_platform.h"
__FBSDID("$FreeBSD: head/lib/libtransform/transform_read_open_fd.c 201103 2009-12-28 03:13:49Z kientzle $");

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "transform.h"
#include "transform_read_private.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

struct read_fd_data {
	int	 fd;
	char use_lseek;
	/* used when we're dealing with a filename */
	int close_fd;
	mode_t st_mode;
	char filename[1];
};

static int	file_close(struct transform *, void *);
static int	file_read(struct transform *, void *, struct transform_read_filter *,
	const void **buff, size_t *avail);
static int64_t	file_skip(struct transform *, void *, struct transform_read_filter *,
	int64_t request);
static int      file_visit_fds(struct transform *, const void *,
	transform_fd_visitor *visitor, const void *visitor_data);
static int
_common_open_fd(struct transform *, int, const char *, size_t, int);

int
transform_read_open_filename(struct transform *t, const char *filename,
	size_t block_size)
{
	int fd;

	if (!filename || filename[0] == '\0') {
		transform_set_error(t, TRANSFORM_ERRNO_PROGRAMMER,
			"null/empty filename passed in");
		return (TRANSFORM_FATAL);
	}
	if(0 > (fd = open(filename, O_RDONLY | O_BINARY))) {
		transform_set_error(t, errno,
			"Failed to open '%s'", filename);
		return (TRANSFORM_FATAL);
	}

	return (_common_open_fd(t, fd, filename, block_size, 1));
}

int
transform_read_open_filename_fd(struct transform *t, const char *filename,
	size_t block_size, int fd)
{
	return (_common_open_fd(t, fd, filename, block_size, 1));
}

int
transform_read_open_fd(struct transform *t, int fd, size_t block_size)
{
	return (_common_open_fd(t, fd, NULL, block_size, 0));
}

static int
_common_open_fd(struct transform *t, int fd, const char *filename,
	size_t block_size, int close_fd)
{
	struct transform_read_filter *source;
	struct stat st;
	struct read_fd_data *mine;

	int is_disk_like = 0;
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
	off_t mediasize = 0; // FreeBSD-specific, so off_t okay here.
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	struct disklabel dl;
#elif defined(__DragonFly__)
	struct partinfo pi;
#endif

	transform_clear_error(t);
	if (fstat(fd, &st) != 0) {
		if (filename) {
			transform_set_error(t, errno, "can't stat '%s'", filename);
		} else {
			transform_set_error(t, errno, "Can't stat fd %d", fd);
		}
		if (close_fd)
			close(fd);
		return (TRANSFORM_FATAL);
	}

	/*
	 * Determine whether the input looks like a disk device or a
	 * tape device.  The results are used below to select an I/O
	 * strategy:
	 *  = "disk-like" devices support arbitrary lseek() and will
	 *    support I/O requests of any size.  So we get easy skipping
	 *    and can cheat on block sizes to get better performance.
	 *  = "tape-like" devices require strict blocking and use
	 *    specialized ioctls for seeking.
	 *  = "socket-like" devices cannot seek at all but can improve
	 *    performance by using nonblocking I/O to read "whatever is
	 *    available right now".
	 *
	 * Right now, we only specially recognize disk-like devices,
	 * but it should be straightforward to add probes and strategy
	 * here for tape-like and socket-like devices.
	 */
	if (S_ISREG(st.st_mode)) {
		/* Regular files act like disks. */
		is_disk_like = 1;
	}
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
	/* FreeBSD: if it supports DIOCGMEDIASIZE ioctl, it's disk-like. */
	else if (S_ISCHR(st.st_mode) &&
	    ioctl(fd, DIOCGMEDIASIZE, &mediasize) == 0 &&
	    mediasize > 0) {
		is_disk_like = 1;
	}
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	/* Net/OpenBSD: if it supports DIOCGDINFO ioctl, it's disk-like. */
	else if ((S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)) &&
	    ioctl(fd, DIOCGDINFO, &dl) == 0 &&
	    dl.d_partitions[DISKPART(st.st_rdev)].p_size > 0) {
		is_disk_like = 1;
	}
#elif defined(__DragonFly__)
	/* DragonFly BSD:  if it supports DIOCGPART ioctl, it's disk-like. */
	else if (S_ISCHR(st.st_mode) &&
	    ioctl(fd, DIOCGPART, &pi) == 0 &&
	    pi.media_size > 0) {
		is_disk_like = 1;
	}
#elif defined(__linux__)
	/* Linux:  All block devices are disk-like. */
	else if (S_ISBLK(st.st_mode) &&
	    lseek(fd, 0, SEEK_CUR) == 0 &&
	    lseek(fd, 0, SEEK_SET) == 0 &&
	    lseek(fd, 0, SEEK_END) > 0 &&
	    lseek(fd, 0, SEEK_SET) == 0) {
		is_disk_like = 1;
	}
#endif
	/* TODO: Add an "is_tape_like" variable and appropriate tests. */

	/* Disk-like devices prefer power-of-two block sizes.  */
	/* Use provided block_size as a guide so users have some control. */
	if (is_disk_like) {
		size_t new_block_size = DEFAULT_READ_BLOCK_SIZE;
		while (new_block_size < block_size
		    && new_block_size < MAX_READ_BUFFER_SIZE)
			new_block_size *= 2;
		block_size = new_block_size;
	}

	if (filename) {
		size_t filename_len = strlen(filename);
		mine = (struct read_fd_data *)calloc(1,
			sizeof(*mine) + filename_len);
		if (mine) {
			memcpy(mine->filename, filename, filename_len);
		}
	} else {
		mine = (struct read_fd_data *)calloc(1, sizeof(*mine));
	}
	if (!mine) {
		transform_set_error(t, ENOMEM, "No memory");
		free(mine);
		if (close_fd)
			close(fd);
		return (TRANSFORM_FATAL);
	}
	mine->close_fd = close_fd;
	mine->fd = fd;
	mine->st_mode = st.st_mode;

#if defined(__CYGWIN__) || defined(_WIN32)
	setmode(mine->fd, O_BINARY);
#endif

	mine->use_lseek = is_disk_like ? 1 : 0;

	source = transform_read_filter_new(mine,
		close_fd ? "source:filename" : "source:fd",
		TRANSFORM_FILTER_NONE,
		file_read,
		file_skip, file_close, file_visit_fds,
		TRANSFORM_FILTER_SOURCE);

	if (!source) {
		transform_set_error(t, ENOMEM,
			"failed allocating filter");
		free(mine);
		if (close_fd)
			close(fd);
		return (TRANSFORM_FATAL);
	}

	if (TRANSFORM_OK != transform_read_filter_set_buffering(source,
		block_size) ||
		TRANSFORM_OK != transform_read_filter_set_alignment(source, 4096)) {
		free(mine);
		free(source);
	}

	return transform_read_open_source(t, source);
}

static int
file_read(struct transform *t, void *client_data, struct transform_read_filter *upstream,
	const void **buff, size_t *bytes_read)
{
	struct read_fd_data *mine = (struct read_fd_data *)client_data;

	for (;;) {
		*bytes_read = read(mine->fd, (void *)*buff, *bytes_read);
		if (*bytes_read < 0) {
			if (errno == EINTR)
				continue;
			if (mine->filename) {
				transform_set_error(t, errno, "Error reading filename %s, fd %d",
					mine->filename, mine->fd);
			} else {
				transform_set_error(t, errno, "Error reading fd %d", mine->fd);
			}
			return (TRANSFORM_FATAL);
		}
		return (*bytes_read == 0 ? TRANSFORM_EOF : TRANSFORM_OK);
	}
}

static int64_t
file_skip(struct transform *t, void *client_data, struct transform_read_filter *upstream,
	int64_t request)
{
	struct read_fd_data *mine = (struct read_fd_data *)client_data;
	off_t old_offset, new_offset;

	(void)upstream;

	if (!mine->use_lseek)
		return 0;

	if (((old_offset = lseek(mine->fd, 0, SEEK_CUR)) >= 0) &&
	    ((new_offset = lseek(mine->fd, request, SEEK_CUR)) >= 0))
		return (new_offset - old_offset);

	/* If seek failed once, it will probably fail again. */
	mine->use_lseek = 0;

	/* Let libtransform recover with read+discard. */
	if (errno == ESPIPE)
		return (0);

	/*
	 * There's been an error other than ESPIPE. This is most
	 * likely caused by a programmer error (too large request)
	 * or a corrupted transform file.
	 */
	if (mine->filename) {
		transform_set_error(t, errno, "Error seeking in file %s, fd %d",
			mine->filename, mine->fd);
	} else {
		transform_set_error(t, errno, "Error seeking in fd %d", mine->fd);
	}
	return (-1);
}

static int
file_close(struct transform *t, void *client_data)
{
	struct read_fd_data *mine = (struct read_fd_data *)client_data;

	(void)t; /* UNUSED */

	/* Only flush and close if open succeeded. */
	if (mine->close_fd && mine->fd >= 0) {
		/*
		 * Sometimes, we should flush the input before closing.
		 *   Regular files: faster to just close without flush.
		 *   Disk-like devices:  Ditto.
		 *   Tapes: must not flush (user might need to
		 *      read the "next" item on a non-rewind device).
		 *   Pipes and sockets:  must flush (otherwise, the
		 *      program feeding the pipe or socket may complain).
		 * Here, I flush everything except for regular files and
		 * device nodes.
		 */
		 /* XXX this seems a little too smart for libtransform... */
		if (!S_ISREG(mine->st_mode)
		    && !S_ISCHR(mine->st_mode)
		    && !S_ISBLK(mine->st_mode)) {
			ssize_t bytesRead;
			void *p = malloc(DEFAULT_READ_BLOCK_SIZE);
			if (p) {
				do {
					bytesRead = read(mine->fd, p, DEFAULT_READ_BLOCK_SIZE);
				} while (bytesRead > 0);
				free(p);
			}
		}
		/* If a named file was opened, then it needs to be closed. */
		if (mine->filename[0] != '\0')
			close(mine->fd);
	}
	free(mine);
	return (TRANSFORM_OK);
}

static int
file_visit_fds(struct transform *transform, const void *_data,
    transform_fd_visitor *visitor, const void *visitor_data)
{
	struct read_fd_data *mine = (struct read_fd_data *)_data;
	return visitor(transform, mine->fd, (void *)visitor_data);
}
