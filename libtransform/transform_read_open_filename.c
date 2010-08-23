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
__FBSDID("$FreeBSD: head/lib/libtransform/transform_read_open_filename.c 201093 2009-12-28 02:28:44Z kientzle $");

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
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
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <sys/disk.h>
#elif defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/disklabel.h>
#include <sys/dkio.h>
#elif defined(__DragonFly__)
#include <sys/diskslice.h>
#endif

#include "transform.h"
#include "transform_read_private.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

struct read_file_data {
	int	 fd;
	size_t	 block_size;
	void	*buffer;
	mode_t	 st_mode;  /* Mode bits for opened file. */
	char	 use_lseek;
	char	 filename[1]; /* Must be last! */
};

static int	file_close(struct transform *, void *);
static ssize_t	file_read(struct transform *, void *, const void **buff);
static int64_t	file_skip(struct transform *, void *, int64_t request);
static off_t	file_skip_lseek(struct transform *, void *, off_t request);
static int      file_visit_fds(struct transform_read_filter *f, int position,
    transform_fd_visitor *visitor, const void *visitor_data);

int
transform_read_open_file(struct transform *a, const char *filename,
    size_t block_size)
{
	return (transform_read_open_filename(a, filename, block_size));
}

int
transform_read_open_filename(struct transform *a, const char *filename,
    size_t block_size)
{
	int fd;

	transform_clear_error(a);
	if (filename != NULL && filename[0] == '\0') {
		/* We used to delegate stdin support by
		 * directly calling transform_read_open_fd(a,0,block_size)
		 * here, but that doesn't (and shouldn't) handle the
		 * end-of-file flush when reading stdout from a pipe.
		 * Basically, read_open_fd() is intended for folks who
		 * are willing to handle such details themselves.  This
		 * API is intended to be a little smarter for folks who
		 * want easy handling of the common case.
		 */
		filename = "";
		fd = 0;
	} else {
		fd = open(filename, O_RDONLY | O_BINARY);
		if (fd < 0) {
			transform_set_error(a, errno,
			    "Failed to open '%s'", filename);
			return (TRANSFORM_FATAL);
		}
	}

	return transform_read_open_filename_fd(a, filename, block_size, fd);
}


/*
 * primarily purpose of this secondary opener is to remove a potential attack
 * against libarchive consumers- libarchive for example, uses this functionality
 * but has to do it's own stating.  It's unlikely, but swapping in a different
 * file in between stat and open raises questions this author doesn't want to
 * deal with.
 * Thus this method, given a filename for an opened fd.
 */
int
transform_read_open_filename_fd(struct transform *a, const char *filename,
    size_t block_size, int fd)
{
	struct stat st;
	struct read_file_data *mine;
	void *buffer;
	int is_disk_like = 0;
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
	off_t mediasize = 0; // FreeBSD-specific, so off_t okay here.
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	struct disklabel dl;
#elif defined(__DragonFly__)
	struct partinfo pi;
#endif

	transform_clear_error(a);
	if (filename == NULL || filename[0] == '\0') {
		/* We used to delegate stdin support by
		 * directly calling transform_read_open_fd(a,0,block_size)
		 * here, but that doesn't (and shouldn't) handle the
		 * end-of-file flush when reading stdout from a pipe.
		 * Basically, read_open_fd() is intended for folks who
		 * are willing to handle such details themselves.  This
		 * API is intended to be a little smarter for folks who
		 * want easy handling of the common case.
		 */
		filename = ""; /* Normalize NULL to "" */
	}

	if (fstat(fd, &st) != 0) {
		transform_set_error(a, errno, "Can't stat '%s'", filename);
		return (TRANSFORM_FATAL);
	}

	/* 
	 * we just force O_BINARY always; it's redundant in light of common
	 * pathways, but structuring it like this avoids any potential gotchas
	 */
	
#if defined(__CYGWIN__) || defined(_WIN32)
	setmode(0, O_BINARY);
#endif

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

	mine = (struct read_file_data *)calloc(1,
	    sizeof(*mine) + strlen(filename));
	/* Disk-like devices prefer power-of-two block sizes.  */
	/* Use provided block_size as a guide so users have some control. */
	if (is_disk_like) {
		size_t new_block_size = 64 * 1024;
		while (new_block_size < block_size
		    && new_block_size < 64 * 1024 * 1024)
			new_block_size *= 2;
		block_size = new_block_size;
	}
	buffer = malloc(block_size);
	if (mine == NULL || buffer == NULL) {
		transform_set_error(a, ENOMEM, "No memory");
		free(mine);
		free(buffer);
		return (TRANSFORM_FATAL);
	}
	strcpy(mine->filename, filename);
	mine->block_size = block_size;
	mine->buffer = buffer;
	mine->fd = fd;
	/* Remember mode so close can decide whether to flush. */
	mine->st_mode = st.st_mode;

	/* Disk-like inputs can use lseek(). */
	if (is_disk_like)
		mine->use_lseek = 1;

	return (transform_read_open2(a, mine,
		NULL, file_read, file_skip, file_close, file_visit_fds));
}

static ssize_t
file_read(struct transform *t, void *client_data, const void **buff)
{
	struct read_file_data *mine = (struct read_file_data *)client_data;
	ssize_t bytes_read;

	/* TODO: If a recent lseek() operation has left us
	 * mis-aligned, read and return a short block to try to get
	 * us back in alignment. */

	/* TODO: Someday, try mmap() here; if that succeeds, give
	 * the entire file to libtransform as a single block.  That
	 * could be a lot faster than block-by-block manual I/O. */

	/* TODO: We might be able to improve performance on pipes and
	 * sockets by setting non-blocking I/O and just accepting
	 * whatever we get here instead of waiting for a full block
	 * worth of data. */

	*buff = mine->buffer;
	for (;;) {
		bytes_read = read(mine->fd, mine->buffer, mine->block_size);
		if (bytes_read < 0) {
			if (errno == EINTR)
				continue;
			else if (mine->filename[0] == '\0')
				transform_set_error(t, errno, "Error reading stdin");
			else
				transform_set_error(t, errno, "Error reading '%s'",
				    mine->filename);
		}
		return (bytes_read);
	}
}

/*
 * Regular files and disk-like block devices can use simple lseek
 * without needing to round the request to the block size.
 *
 * TODO: This can leave future reads mis-aligned.  Since we know the
 * offset here, we should store it and use it in file_read() above
 * to determine whether we should perform a short read to get back
 * into alignment.  Long series of mis-aligned reads can negatively
 * impact disk throughput.  (Of course, the performance impact should
 * be carefully tested; extra code complexity is only worthwhile if
 * it does provide measurable improvement.)
 *
 * TODO: Be lazy about the actual seek.  There are a few pathological
 * cases where libtransform makes a bunch of seek requests in a row
 * without any intervening reads.  This isn't a huge performance
 * problem, since the kernel handles seeks lazily already, but
 * it would be very slightly faster if we simply remembered the
 * seek request here and then actually performed the seek at the
 * top of the read callback above.
 */
static off_t
file_skip_lseek(struct transform *t, void *client_data, off_t request)
{
	struct read_file_data *mine = (struct read_file_data *)client_data;
	off_t old_offset, new_offset;

	if ((old_offset = lseek(mine->fd, 0, SEEK_CUR)) >= 0 &&
	    (new_offset = lseek(mine->fd, request, SEEK_CUR)) >= 0)
		return (new_offset - old_offset);

	/* If lseek() fails, don't bother trying again. */
	mine->use_lseek = 0;

	/* Let libtransform recover with read+discard */
	if (errno == ESPIPE)
		return (0);

	/* If the input is corrupted or truncated, fail. */
	if (mine->filename[0] == '\0')
		/* Shouldn't happen; lseek() on stdin should raise ESPIPE. */
		transform_set_error(t, errno, "Error seeking in stdin");
	else
		transform_set_error(t, errno, "Error seeking in '%s'",
		    mine->filename);
	return (-1);
}


/*
 * TODO: Implement another file_skip_XXXX that uses MTIO ioctls to
 * accelerate operation on tape drives.
 */

static int64_t
file_skip(struct transform *t, void *client_data, int64_t request)
{
	struct read_file_data *mine = (struct read_file_data *)client_data;

	/* Delegate skip requests. */
	if (mine->use_lseek)
		return (file_skip_lseek(t, client_data, request));

	/* If we can't skip, return 0; libtransform will read+discard instead. */
	return (0);
}

static int
file_close(struct transform *t, void *client_data)
{
	struct read_file_data *mine = (struct read_file_data *)client_data;

	(void)t; /* UNUSED */

	/* Only flush and close if open succeeded. */
	if (mine->fd >= 0) {
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
		if (!S_ISREG(mine->st_mode)
		    && !S_ISCHR(mine->st_mode)
		    && !S_ISBLK(mine->st_mode)) {
			ssize_t bytesRead;
			do {
				bytesRead = read(mine->fd, mine->buffer,
				    mine->block_size);
			} while (bytesRead > 0);
		}
		/* If a named file was opened, then it needs to be closed. */
		if (mine->filename[0] != '\0')
			close(mine->fd);
	}
	free(mine->buffer);
	free(mine);
	return (TRANSFORM_OK);
}

static int
file_visit_fds(struct transform_read_filter *f, int position,
    transform_fd_visitor *visitor, const void *visitor_data)
{
	struct read_file_data *mine = (struct read_file_data *)f->data;
	return visitor((struct transform *)f->transform,
		position, mine->fd, (void *)visitor_data);
}
