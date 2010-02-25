/*-
 * Copyright (c) 2003-2009 Tim Kientzle
 * Copyright (c) 2010 Michihiro NAKAJIMA
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
__FBSDID("$FreeBSD: head/lib/libarchive/archive_read_disk_entry_from_file.c 201084 2009-12-28 02:14:09Z kientzle $");

#ifdef HAVE_SYS_TYPES_H
/* Mac OSX requires sys/types.h before sys/acl.h. */
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_ACL_H
#include <sys/acl.h>
#endif
#ifdef HAVE_SYS_EXTATTR_H
#include <sys/extattr.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif
#ifdef HAVE_ACL_LIBACL_H
#include <acl/libacl.h>
#endif
#ifdef HAVE_ATTR_XATTR_H
#include <attr/xattr.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LINUX_FIEMAP_H
#include <linux/fiemap.h>
#endif
#ifdef HAVE_LINUX_FS_H
#include <linux/fs.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#if defined(HAVE_WINIOCTL_H) && !defined(__CYGWIN__)
#include <winioctl.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_disk_private.h"

/*
 * Linux and FreeBSD plug this obvious hole in POSIX.1e in
 * different ways.
 */
#if HAVE_ACL_GET_PERM
#define	ACL_GET_PERM acl_get_perm
#elif HAVE_ACL_GET_PERM_NP
#define	ACL_GET_PERM acl_get_perm_np
#endif

static int setup_acls_posix1e(struct archive_read_disk *,
    struct archive_entry *, int fd);
static int setup_xattrs(struct archive_read_disk *,
    struct archive_entry *, int fd);
static int setup_sparse(struct archive_read_disk *,
    struct archive_entry *, int fd);

int
archive_read_disk_entry_from_file(struct archive *_a,
    struct archive_entry *entry,
    int fd, const struct stat *st)
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;
	const char *path, *name;
	struct stat s;
	int initial_fd = fd;
	int r, r1;

	archive_clear_error(_a);
	path = archive_entry_sourcepath(entry);
	if (path == NULL)
		path = archive_entry_pathname(entry);

#ifdef EXT2_IOC_GETFLAGS
	/* Linux requires an extra ioctl to pull the flags.  Although
	 * this is an extra step, it has a nice side-effect: We get an
	 * open file descriptor which we can use in the subsequent lookups. */
	if ((S_ISREG(st->st_mode) || S_ISDIR(st->st_mode))) {
		if (fd < 0)
			fd = open(pathname, O_RDONLY | O_NONBLOCK | O_BINARY);
		if (fd >= 0) {
			unsigned long stflags;
			int r = ioctl(fd, EXT2_IOC_GETFLAGS, &stflags);
			if (r == 0 && stflags != 0)
				archive_entry_set_fflags(entry, stflags, 0);
		}
	}
#endif

	if (st == NULL) {
		/* TODO: On Windows, use GetFileInfoByHandle() here.
		 * Using Windows stat() call is badly broken, but
		 * even the stat() wrapper has problems because
		 * 'struct stat' is broken on Windows.
		 */
#if HAVE_FSTAT
		if (fd >= 0) {
			if (fstat(fd, &s) != 0) {
				archive_set_error(&a->archive, errno,
				    "Can't fstat");
				return (ARCHIVE_FAILED);
			}
		} else
#endif
#if HAVE_LSTAT
		if (!a->follow_symlinks) {
			if (lstat(path, &s) != 0) {
				archive_set_error(&a->archive, errno,
				    "Can't lstat %s", path);
				return (ARCHIVE_FAILED);
			}
		} else
#endif
		if (stat(path, &s) != 0) {
			archive_set_error(&a->archive, errno,
			    "Can't stat %s", path);
			return (ARCHIVE_FAILED);
		}
		st = &s;
	}
	archive_entry_copy_stat(entry, st);

	/* Lookup uname/gname */
	name = archive_read_disk_uname(_a, archive_entry_uid(entry));
	if (name != NULL)
		archive_entry_copy_uname(entry, name);
	name = archive_read_disk_gname(_a, archive_entry_gid(entry));
	if (name != NULL)
		archive_entry_copy_gname(entry, name);

#ifdef HAVE_STRUCT_STAT_ST_FLAGS
	/* On FreeBSD, we get flags for free with the stat. */
	/* TODO: Does this belong in copy_stat()? */
	if (st->st_flags != 0)
		archive_entry_set_fflags(entry, st->st_flags, 0);
#endif

#ifdef HAVE_READLINK
	if (S_ISLNK(st->st_mode)) {
		size_t linkbuffer_len = st->st_size + 1;
		char *linkbuffer;
		int lnklen;

		linkbuffer = malloc(linkbuffer_len);
		if (linkbuffer == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "Couldn't read link data");
			return (ARCHIVE_FAILED);
		}
		lnklen = readlink(path, linkbuffer, linkbuffer_len);
		if (lnklen < 0) {
			archive_set_error(&a->archive, errno,
			    "Couldn't read link data");
			free(linkbuffer);
			return (ARCHIVE_FAILED);
		}
		linkbuffer[lnklen] = 0;
		archive_entry_set_symlink(entry, linkbuffer);
		free(linkbuffer);
	}
#endif

	r = setup_acls_posix1e(a, entry, fd);
	r1 = setup_xattrs(a, entry, fd);
	if (r1 < r)
		r = r1;
	if (S_ISREG(st->st_mode) && archive_entry_size(entry) > 0 &&
	    archive_entry_hardlink(entry) == NULL) {
		r1 = setup_sparse(a, entry, fd);
		if (r1 < r)
			r = r1;
	}
	/* If we opened the file earlier in this function, close it. */
	if (initial_fd != fd)
		close(fd);
	return (r);
}

#ifdef HAVE_POSIX_ACL
static void setup_acl_posix1e(struct archive_read_disk *a,
    struct archive_entry *entry, acl_t acl, int archive_entry_acl_type);

static int
setup_acls_posix1e(struct archive_read_disk *a,
    struct archive_entry *entry, int fd)
{
	const char	*accpath;
	acl_t		 acl;

	accpath = archive_entry_sourcepath(entry);
	if (accpath == NULL)
		accpath = archive_entry_pathname(entry);

	archive_entry_acl_clear(entry);

	/* Retrieve access ACL from file. */
	if (fd >= 0)
		acl = acl_get_fd(fd);
#if HAVE_ACL_GET_LINK_NP
	else if (!a->follow_symlinks)
		acl = acl_get_link_np(accpath, ACL_TYPE_ACCESS);
#endif
	else
		acl = acl_get_file(accpath, ACL_TYPE_ACCESS);
	if (acl != NULL) {
		setup_acl_posix1e(a, entry, acl,
		    ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
		acl_free(acl);
	}

	/* Only directories can have default ACLs. */
	if (S_ISDIR(archive_entry_mode(entry))) {
		acl = acl_get_file(accpath, ACL_TYPE_DEFAULT);
		if (acl != NULL) {
			setup_acl_posix1e(a, entry, acl,
			    ARCHIVE_ENTRY_ACL_TYPE_DEFAULT);
			acl_free(acl);
		}
	}
	return (ARCHIVE_OK);
}

/*
 * Translate POSIX.1e ACL into libarchive internal structure.
 */
static void
setup_acl_posix1e(struct archive_read_disk *a,
    struct archive_entry *entry, acl_t acl, int archive_entry_acl_type)
{
	acl_tag_t	 acl_tag;
	acl_entry_t	 acl_entry;
	acl_permset_t	 acl_permset;
	int		 s, ae_id, ae_tag, ae_perm;
	const char	*ae_name;

	s = acl_get_entry(acl, ACL_FIRST_ENTRY, &acl_entry);
	while (s == 1) {
		ae_id = -1;
		ae_name = NULL;

		acl_get_tag_type(acl_entry, &acl_tag);
		if (acl_tag == ACL_USER) {
			ae_id = (int)*(uid_t *)acl_get_qualifier(acl_entry);
			ae_name = archive_read_disk_uname(&a->archive, ae_id);
			ae_tag = ARCHIVE_ENTRY_ACL_USER;
		} else if (acl_tag == ACL_GROUP) {
			ae_id = (int)*(gid_t *)acl_get_qualifier(acl_entry);
			ae_name = archive_read_disk_gname(&a->archive, ae_id);
			ae_tag = ARCHIVE_ENTRY_ACL_GROUP;
		} else if (acl_tag == ACL_MASK) {
			ae_tag = ARCHIVE_ENTRY_ACL_MASK;
		} else if (acl_tag == ACL_USER_OBJ) {
			ae_tag = ARCHIVE_ENTRY_ACL_USER_OBJ;
		} else if (acl_tag == ACL_GROUP_OBJ) {
			ae_tag = ARCHIVE_ENTRY_ACL_GROUP_OBJ;
		} else if (acl_tag == ACL_OTHER) {
			ae_tag = ARCHIVE_ENTRY_ACL_OTHER;
		} else {
			/* Skip types that libarchive can't support. */
			continue;
		}

		acl_get_permset(acl_entry, &acl_permset);
		ae_perm = 0;
		/*
		 * acl_get_perm() is spelled differently on different
		 * platforms; see above.
		 */
		if (ACL_GET_PERM(acl_permset, ACL_EXECUTE))
			ae_perm |= ARCHIVE_ENTRY_ACL_EXECUTE;
		if (ACL_GET_PERM(acl_permset, ACL_READ))
			ae_perm |= ARCHIVE_ENTRY_ACL_READ;
		if (ACL_GET_PERM(acl_permset, ACL_WRITE))
			ae_perm |= ARCHIVE_ENTRY_ACL_WRITE;

		archive_entry_acl_add_entry(entry,
		    archive_entry_acl_type, ae_perm, ae_tag,
		    ae_id, ae_name);

		s = acl_get_entry(acl, ACL_NEXT_ENTRY, &acl_entry);
	}
}
#else
static int
setup_acls_posix1e(struct archive_read_disk *a,
    struct archive_entry *entry, int fd)
{
	(void)a;      /* UNUSED */
	(void)entry;  /* UNUSED */
	(void)fd;     /* UNUSED */
	return (ARCHIVE_OK);
}
#endif

#if HAVE_LISTXATTR && HAVE_LLISTXATTR && HAVE_GETXATTR && HAVE_LGETXATTR

/*
 * Linux extended attribute support.
 *
 * TODO:  By using a stack-allocated buffer for the first
 * call to getxattr(), we might be able to avoid the second
 * call entirely.  We only need the second call if the
 * stack-allocated buffer is too small.  But a modest buffer
 * of 1024 bytes or so will often be big enough.  Same applies
 * to listxattr().
 */


static int
setup_xattr(struct archive_read_disk *a,
    struct archive_entry *entry, const char *name, int fd)
{
	ssize_t size;
	void *value = NULL;
	const char *accpath;

	(void)fd; /* UNUSED */

	accpath = archive_entry_sourcepath(entry);
	if (accpath == NULL)
		accpath = archive_entry_pathname(entry);

	if (!a->follow_symlinks)
		size = lgetxattr(accpath, name, NULL, 0);
	else
		size = getxattr(accpath, name, NULL, 0);

	if (size == -1) {
		archive_set_error(&a->archive, errno,
		    "Couldn't query extended attribute");
		return (ARCHIVE_WARN);
	}

	if (size > 0 && (value = malloc(size)) == NULL) {
		archive_set_error(&a->archive, errno, "Out of memory");
		return (ARCHIVE_FATAL);
	}

	if (!a->follow_symlinks)
		size = lgetxattr(accpath, name, value, size);
	else
		size = getxattr(accpath, name, value, size);

	if (size == -1) {
		archive_set_error(&a->archive, errno,
		    "Couldn't read extended attribute");
		return (ARCHIVE_WARN);
	}

	archive_entry_xattr_add_entry(entry, name, value, size);

	free(value);
	return (ARCHIVE_OK);
}

static int
setup_xattrs(struct archive_read_disk *a,
    struct archive_entry *entry, int fd)
{
	char *list, *p;
	const char *path;
	ssize_t list_size;


	path = archive_entry_sourcepath(entry);
	if (path == NULL)
		path = archive_entry_pathname(entry);

	if (!a->follow_symlinks)
		list_size = llistxattr(path, NULL, 0);
	else
		list_size = listxattr(path, NULL, 0);

	if (list_size == -1) {
		if (errno == ENOTSUP)
			return (ARCHIVE_OK);
		archive_set_error(&a->archive, errno,
			"Couldn't list extended attributes");
		return (ARCHIVE_WARN);
	}

	if (list_size == 0)
		return (ARCHIVE_OK);

	if ((list = malloc(list_size)) == NULL) {
		archive_set_error(&a->archive, errno, "Out of memory");
		return (ARCHIVE_FATAL);
	}

	if (!a->follow_symlinks)
		list_size = llistxattr(path, list, list_size);
	else
		list_size = listxattr(path, list, list_size);

	if (list_size == -1) {
		archive_set_error(&a->archive, errno,
			"Couldn't retrieve extended attributes");
		free(list);
		return (ARCHIVE_WARN);
	}

	for (p = list; (p - list) < list_size; p += strlen(p) + 1) {
		if (strncmp(p, "system.", 7) == 0 ||
				strncmp(p, "xfsroot.", 8) == 0)
			continue;
		setup_xattr(a, entry, p, fd);
	}

	free(list);
	return (ARCHIVE_OK);
}

#elif HAVE_EXTATTR_GET_FILE && HAVE_EXTATTR_LIST_FILE

/*
 * FreeBSD extattr interface.
 */

/* TODO: Implement this.  Follow the Linux model above, but
 * with FreeBSD-specific system calls, of course.  Be careful
 * to not include the system extattrs that hold ACLs; we handle
 * those separately.
 */
static int
setup_xattr(struct archive_read_disk *a, struct archive_entry *entry,
    int namespace, const char *name, const char *fullname, int fd);

static int
setup_xattr(struct archive_read_disk *a, struct archive_entry *entry,
    int namespace, const char *name, const char *fullname, int fd)
{
	ssize_t size;
	void *value = NULL;
	const char *accpath;

	(void)fd; /* UNUSED */

	accpath = archive_entry_sourcepath(entry);
	if (accpath == NULL)
		accpath = archive_entry_pathname(entry);

	if (!a->follow_symlinks)
		size = extattr_get_link(accpath, namespace, name, NULL, 0);
	else
		size = extattr_get_file(accpath, namespace, name, NULL, 0);

	if (size == -1) {
		archive_set_error(&a->archive, errno,
		    "Couldn't query extended attribute");
		return (ARCHIVE_WARN);
	}

	if (size > 0 && (value = malloc(size)) == NULL) {
		archive_set_error(&a->archive, errno, "Out of memory");
		return (ARCHIVE_FATAL);
	}

	if (!a->follow_symlinks)
		size = extattr_get_link(accpath, namespace, name, value, size);
	else
		size = extattr_get_file(accpath, namespace, name, value, size);

	if (size == -1) {
		archive_set_error(&a->archive, errno,
		    "Couldn't read extended attribute");
		return (ARCHIVE_WARN);
	}

	archive_entry_xattr_add_entry(entry, fullname, value, size);

	free(value);
	return (ARCHIVE_OK);
}

static int
setup_xattrs(struct archive_read_disk *a,
    struct archive_entry *entry, int fd)
{
	char buff[512];
	char *list, *p;
	ssize_t list_size;
	const char *path;
	int namespace = EXTATTR_NAMESPACE_USER;

	path = archive_entry_sourcepath(entry);
	if (path == NULL)
		path = archive_entry_pathname(entry);

	if (!a->follow_symlinks)
		list_size = extattr_list_link(path, namespace, NULL, 0);
	else
		list_size = extattr_list_file(path, namespace, NULL, 0);

	if (list_size == -1 && errno == EOPNOTSUPP)
		return (ARCHIVE_OK);
	if (list_size == -1) {
		archive_set_error(&a->archive, errno,
			"Couldn't list extended attributes");
		return (ARCHIVE_WARN);
	}

	if (list_size == 0)
		return (ARCHIVE_OK);

	if ((list = malloc(list_size)) == NULL) {
		archive_set_error(&a->archive, errno, "Out of memory");
		return (ARCHIVE_FATAL);
	}

	if (!a->follow_symlinks)
		list_size = extattr_list_link(path, namespace, list, list_size);
	else
		list_size = extattr_list_file(path, namespace, list, list_size);

	if (list_size == -1) {
		archive_set_error(&a->archive, errno,
			"Couldn't retrieve extended attributes");
		free(list);
		return (ARCHIVE_WARN);
	}

	p = list;
	while ((p - list) < list_size) {
		size_t len = 255 & (int)*p;
		char *name;

		strcpy(buff, "user.");
		name = buff + strlen(buff);
		memcpy(name, p + 1, len);
		name[len] = '\0';
		setup_xattr(a, entry, namespace, name, buff, fd);
		p += 1 + len;
	}

	free(list);
	return (ARCHIVE_OK);
}

#else

/*
 * Generic (stub) extended attribute support.
 */
static int
setup_xattrs(struct archive_read_disk *a,
    struct archive_entry *entry, int fd)
{
	(void)a;     /* UNUSED */
	(void)entry; /* UNUSED */
	(void)fd;    /* UNUSED */
	return (ARCHIVE_OK);
}

#endif

#if defined(HAVE_LINUX_FIEMAP_H)

/*
 * Linux sparse interface.
 */

static int
setup_sparse(struct archive_read_disk *a,
    struct archive_entry *entry, int fd)
{
	char buff[4096];
	struct fiemap *fm;
	struct fiemap_extent *fe;
	int64_t size;
	int count, do_fiemap;
	int initial_fd = fd;
	int exit_sts = ARCHIVE_OK;

	if (fd < 0) {
		const char *path;

		path = archive_entry_sourcepath(entry);
		if (path == NULL)
			path = archive_entry_pathname(entry);
		fd = open(path, O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			archive_set_error(&a->archive, errno,
			    "Can't open `%s'", path);
			return (ARCHIVE_FAILED);
		}
	}

	count = (sizeof(buff) - sizeof(*fm))/sizeof(*fe);
	fm = (struct fiemap *)buff;
	fm->fm_start = 0;
	fm->fm_length = ~0ULL;;
	fm->fm_flags = FIEMAP_FLAG_SYNC;
	fm->fm_extent_count = count;
	do_fiemap = 1;
	size = archive_entry_size(entry);
	for (;;) {
		int i, r;

		r = ioctl(fd, FS_IOC_FIEMAP, fm); 
		if (r < 0) {
			/* When errno is ENOTTY, it is better we should
			 * return ARCHIVE_OK because an earlier version
			 *(<2.6.28) cannot perfom FS_IOC_FIEMAP */
			if (errno != ENOTTY) {
				archive_set_error(&a->archive, errno,
				    "FIEMAP failed");
				exit_sts = ARCHIVE_FAILED;
			}
			goto exit_setup_sparse;
		}
		if (fm->fm_mapped_extents == 0)
			break;
		fe = fm->fm_extents;
		for (i = 0; i < fm->fm_mapped_extents; i++, fe++) {
			if (!(fe->fe_flags & FIEMAP_EXTENT_UNWRITTEN)) {
				/* The fe_length of the last block does not
				 * adjust itself to its size files. */
				int64_t length = fe->fe_length;
				if (fe->fe_logical + length > size)
					length -= fe->fe_logical + length - size;
				if (fe->fe_logical == 0 && length == size) {
					/* This is not sparse. */
					do_fiemap = 0;
					break;
				}
				if (length > 0)
					archive_entry_sparse_add_entry(entry,
					    fe->fe_logical, length);
			}
			if (fe->fe_flags & FIEMAP_EXTENT_LAST)
				do_fiemap = 0;
		}
		if (do_fiemap) {
			fe = fm->fm_extents + fm->fm_mapped_extents -1;
			fm->fm_start = fe->fe_logical + fe->fe_length;
		} else
			break;
	}
exit_setup_sparse:
	if (initial_fd != fd)
		close(fd);
	return (exit_sts);
}

#elif defined(SEEK_HOLE) && defined(SEEK_DATA) && defined(_PC_MIN_HOLE_SIZE)

/*
 * FreeBSD and Solaris sparse interface.
 */

static int
setup_sparse(struct archive_read_disk *a,
    struct archive_entry *entry, int fd)
{
	int64_t size;
	int initial_fd = fd;
	off_t initial_off;
	off_t off_s, off_e;
	int exit_sts = ARCHIVE_OK;

	/* Does filesystem support the reporting of hole ? */
	if (fd >= 0) {
		if (fpathconf(fd, _PC_MIN_HOLE_SIZE) <= 0)
			return (ARCHIVE_OK);
		initial_off = lseek(fd, 0, SEEK_CUR);
		if (initial_off != 0)
			lseek(fd, 0, SEEK_SET);
	} else {
		const char *path;

		path = archive_entry_sourcepath(entry);
		if (path == NULL)
			path = archive_entry_pathname(entry);
		if (pathconf(path, _PC_MIN_HOLE_SIZE) <= 0)
			return (ARCHIVE_OK);
		fd = open(path, O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			archive_set_error(&a->archive, errno,
			    "Can't open `%s'", path);
			return (ARCHIVE_FAILED);
		}
		initial_off = 0;
	}

	off_s = 0;
	size = archive_entry_size(entry);
	while (off_s < size) {
		off_s = lseek(fd, off_s, SEEK_DATA);
		if (off_s == (off_t)-1) {
			if (errno == ENXIO)
				break;/* no more hole */
			archive_set_error(&a->archive, errno,
			    "lseek(SEEK_HOLE) failed");
			exit_sts = ARCHIVE_FAILED;
			goto exit_setup_sparse;
		}
		off_e = lseek(fd, off_s, SEEK_HOLE);
		if (off_s == (off_t)-1) {
			if (errno == ENXIO) {
				off_e = lseek(fd, 0, SEEK_END);
				if (off_e != (off_t)-1)
					break;/* no more data */
			}
			archive_set_error(&a->archive, errno,
			    "lseek(SEEK_DATA) failed");
			exit_sts = ARCHIVE_FAILED;
			goto exit_setup_sparse;
		}
		if (off_s == 0 && off_e == size)
			break;/* This is not spase. */
		archive_entry_sparse_add_entry(entry, off_s,
			off_e - off_s);
		off_s = off_e;
	}
exit_setup_sparse:
	if (initial_fd != fd)
		close(fd);
	else
		lseek(fd, initial_off, SEEK_SET);
	return (exit_sts);
}

#elif defined(_WIN32) && !defined(__CYGWIN__)

/*
 * Windows sparse interface.
 */
#if defined(__MINGW32__) && !defined(FSCTL_QUERY_ALLOCATED_RANGES)
#define FSCTL_QUERY_ALLOCATED_RANGES 0x940CF
typedef struct {
	LARGE_INTEGER FileOffset;
	LARGE_INTEGER Length;
} FILE_ALLOCATED_RANGE_BUFFER;
#endif

static int
setup_sparse(struct archive_read_disk *a,
    struct archive_entry *entry, int fd)
{
	HANDLE handle;
	BY_HANDLE_FILE_INFORMATION info;
	FILE_ALLOCATED_RANGE_BUFFER range, *outranges = NULL;
	size_t outranges_size;
	LARGE_INTEGER fsize;
	int initial_fd = fd;
	int exit_sts = ARCHIVE_OK;

	if (fd < 0) {
		const char *path;

		path = archive_entry_sourcepath(entry);
		if (path == NULL)
			path = archive_entry_pathname(entry);
		fd = open(path, O_RDONLY | O_BINARY);
		if (fd < 0) {
			archive_set_error(&a->archive, errno,
			    "Can't open `%s'", path);
			return (ARCHIVE_FAILED);
		}
	}
	handle = (HANDLE)_get_osfhandle(fd);
	ZeroMemory(&info, sizeof(info));
	if (!GetFileInformationByHandle (handle, &info)) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			"GetFileInformationByHandle Failed: %lu",
		    GetLastError());
		exit_sts = ARCHIVE_FAILED;
		goto exit_setup_sparse;
	}
	if ((info.dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE) == 0)
		goto exit_setup_sparse;/* Not sparse file */

	fsize.HighPart = info.nFileSizeHigh;
	fsize.LowPart = info.nFileSizeLow;
	range.FileOffset.QuadPart = 0;
	range.Length.QuadPart = fsize.QuadPart;
	outranges_size = 2048;
	outranges = (FILE_ALLOCATED_RANGE_BUFFER *)malloc(outranges_size);
	if (outranges == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			"Couldn't allocate memory");
		exit_sts = ARCHIVE_FATAL;
		goto exit_setup_sparse;
	}

	for (;;) {
		DWORD retbytes;
		BOOL ret;

		for (;;) {
			ret = DeviceIoControl(handle,
			    FSCTL_QUERY_ALLOCATED_RANGES,
			    &range, sizeof(range), outranges,
			    outranges_size, &retbytes, NULL);
			if (ret == 0 && GetLastError() == ERROR_MORE_DATA) {
				free(outranges);
				outranges_size *= 2;
				outranges = (FILE_ALLOCATED_RANGE_BUFFER *)
				    malloc(outranges_size);
				if (outranges == NULL) {
					archive_set_error(&a->archive,
					    ARCHIVE_ERRNO_MISC,
					    "Couldn't allocate memory");
					exit_sts = ARCHIVE_FATAL;
					goto exit_setup_sparse;
				}
				continue;
			} else
				break;
		}
		if (ret != 0) {
			if (retbytes > 0) {
				DWORD i, n;

				n = retbytes / sizeof(outranges[0]);
				if (n == 1 &&
				    outranges[0].FileOffset.QuadPart == 0 &&
				    outranges[0].Length.QuadPart == fsize.QuadPart)
					break;/* This is not sparse. */
				for (i = 0; i < n; i++)
					archive_entry_sparse_add_entry(entry,
					    outranges[i].FileOffset.QuadPart,
						outranges[i].Length.QuadPart);
				range.FileOffset.QuadPart =
				    outranges[n-1].FileOffset.QuadPart
				    + outranges[n-1].Length.QuadPart;
				range.Length.QuadPart =
				    fsize.QuadPart - range.FileOffset.QuadPart;
				if (range.Length.QuadPart > 0)
					continue;
			} else {
				/* The remaining data is hole. */
				archive_entry_sparse_add_entry(entry,
				    range.FileOffset.QuadPart,
				    range.Length.QuadPart);
			}
			break;
		} else {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "DeviceIoControl Failed: %lu", GetLastError());
			exit_sts = ARCHIVE_FAILED;
			goto exit_setup_sparse;
		}
	}
exit_setup_sparse:
	if (initial_fd != fd)
		close(fd);
	free(outranges);

	return (exit_sts);
}

#else

/*
 * Generic (stub) sparse support.
 */
static int
setup_sparse(struct archive_read_disk *a,
    struct archive_entry *entry, int fd)
{
	(void)a;     /* UNUSED */
	(void)entry; /* UNUSED */
	(void)fd;    /* UNUSED */
	return (ARCHIVE_OK);
}

#endif