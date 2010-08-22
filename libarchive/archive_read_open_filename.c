/*-
 * Copyright (c) 2003-2010 Tim Kientzle
 * Copyright (c) 2010 Brian Harring
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

#include "archive_platform.h"
__FBSDID("$FreeBSD: head/lib/libarchive/archive_read_open_filename.c 201093 2009-12-28 02:28:44Z kientzle $");

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

#include "archive.h"
#include "archive_private.h"

int
archive_read_open_file(struct archive *a, const char *filename,
    size_t block_size)
{
	return (archive_read_open_filename(a, filename, block_size));
}

int
archive_read_open_filename(struct archive *a, const char *filename,
    size_t block_size)
{
	struct stat st;
	int fd;
	int e;

	archive_clear_error(a);
	if (filename == NULL || filename[0] == '\0') {
		/* We used to delegate stdin support by
		 * directly calling archive_read_open_fd(a,0,block_size)
		 * here, but that doesn't (and shouldn't) handle the
		 * end-of-file flush when reading stdout from a pipe.
		 * Basically, read_open_fd() is intended for folks who
		 * are willing to handle such details themselves.  This
		 * API is intended to be a little smarter for folks who
		 * want easy handling of the common case.
		 */
		filename = ""; /* Normalize NULL to "" */
		fd = 0;
	} else {
		fd = open(filename, O_RDONLY);
		if (fd < 0) {
			archive_set_error(a, errno,
			    "Failed to open '%s'", filename);
			return (ARCHIVE_FATAL);
		}
	}
	if (fstat(fd, &st) != 0) {
		archive_set_error(a, errno, "Can't stat '%s'", filename);
		return (ARCHIVE_FATAL);
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
		/* Safety:  Tell the extractor not to overwrite the input. */
		archive_read_extract_set_skip_file(a, st.st_dev, st.st_ino);
	}

	/* TODO: Add an "is_tape_like" variable and appropriate tests. */

	e = __convert_transform_error_to_archive_error(a, a->transform,
	    transform_read_open_filename_fd(a->transform, filename,
	    	block_size, fd));

	if (ARCHIVE_OK == e) {
		e = archive_read_open_preopened_transform(a);
	}

	return (e);
}
