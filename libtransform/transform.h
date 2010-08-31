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
 *
 * $FreeBSD: src/lib/libtransform/transform.h.in,v 1.50 2008/05/26 17:00:22 kientzle Exp $
 */

#ifndef TRANSFORM_H_INCLUDED
#define	TRANSFORM_H_INCLUDED

/*
 * Note: transform.h is for use outside of libtransform; the configuration
 * headers (config.h, transform_platform.h, etc.) are purely internal.
 * Do NOT use HAVE_XXX configuration macros to control the behavior of
 * this header!  If you must conditionalize, use predefined compiler and/or
 * platform macros.
 */
#if defined(__BORLANDC__) && __BORLANDC__ >= 0x560
# define __LA_STDINT_H <stdint.h>
#elif !defined(__WATCOMC__) && !defined(_MSC_VER) && !defined(__INTERIX) && !defined(__BORLANDC__)
# define __LA_STDINT_H <inttypes.h>
#endif

#include <sys/stat.h>
#ifdef __LA_STDINT_H
# include __LA_STDINT_H /* int64_t, etc. */
#endif
#include <stdio.h> /* For FILE * */

/* Get appropriate definitions of standard POSIX-style types. */
/* These should match the types used in 'struct stat' */
#if defined(_WIN32) && !defined(__CYGWIN__)
#define	__LA_INT64_T	__int64
# if defined(_SSIZE_T_DEFINED)
#  define	__LA_SSIZE_T	ssize_t
# elif defined(_WIN64)
#  define	__LA_SSIZE_T	__int64
# else
#  define	__LA_SSIZE_T	long
# endif
# if defined(__BORLANDC__)
#  define	__LA_UID_T	uid_t
#  define	__LA_GID_T	gid_t
# else
#  define	__LA_UID_T	short
#  define	__LA_GID_T	short
# endif
#else
#include <unistd.h>  /* ssize_t, uid_t, and gid_t */
#define	__LA_INT64_T	int64_t
#define	__LA_SSIZE_T	ssize_t
#define	__LA_UID_T	uid_t
#define	__LA_GID_T	gid_t
#endif

/*
 * On Windows, define LIBTRANSFORM_STATIC if you're building or using a
 * .lib.  The default here assumes you're building a DLL.  Only
 * libtransform source should ever define __LIBTRANSFORM_BUILD.
 */
#if ((defined __WIN32__) || (defined _WIN32) || defined(__CYGWIN__)) && (!defined LIBTRANSFORM_STATIC)
# ifdef __LIBTRANSFORM_BUILD
#  ifdef __GNUC__
#   define __LA_DECL	__attribute__((dllexport)) extern
#  else
#   define __LA_DECL	__declspec(dllexport)
#  endif
# else
#  ifdef __GNUC__
#   define __LA_DECL
#  else
#   define __LA_DECL	__declspec(dllimport)
#  endif
# endif
#else
/* Static libraries or non-Windows needs no special declaration. */
# define __LA_DECL
#endif

#if defined(__GNUC__) && __GNUC__ >= 3
#define	__LA_PRINTF(fmtarg, firstvararg) \
	__attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#else
#define	__LA_PRINTF(fmtarg, firstvararg)	/* nothing */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The version number is provided as both a macro and a function.
 * The macro identifies the installed header; the function identifies
 * the library version (which may not be the same if you're using a
 * dynamically-linked version of the library).  Of course, if the
 * header and library are very different, you should expect some
 * strangeness.  Don't do that.
 */
#define	TRANSFORM_VERSION_NUMBER 3000000
__LA_DECL int		transform_version_number(void);

/*
 * Textual name/version of the library, useful for version displays.
 */
#define	TRANSFORM_VERSION_STRING "libtransform 3.0.0a"
__LA_DECL const char *	transform_version_string(void);

/* Declare our basic types. */
struct transform;

/*
 * Error codes: Use transform_errno() and transform_error_string()
 * to retrieve details.  Unless specified otherwise, all functions
 * that return 'int' use these codes.
 */

/* XXX note these values have intentionally been shifted- this should
 help catch any issues where conversion from transform to archive
 doesn't occur */
#define	TRANSFORM_EOF	  (-1)	/* Found end of transform. */
#define	TRANSFORM_OK	  0	/* Operation was successful. */
#define	TRANSFORM_WARN	(-200)	/* Partial success. */
/* for example, if data can't be pushed for some reason- buffer is full.  retry. */
#define TRANSFORM_FAILED (-250) /* Current operation cannot complete. */
/* fatal cannot be continued however; network buffering, the network connection was lost for example */
#define	TRANSFORM_FATAL	(-300)	/* No more operations are possible. */

/*
 * As far as possible, transform_errno returns standard platform errno codes.
 * Of course, the details vary by platform, so the actual definitions
 * here are stored in "transform_platform.h".  The symbols are listed here
 * for reference; as a rule, clients should not need to know the exact
 * platform-dependent error code.
 */
/* Unrecognized or invalid file format. */
/* #define	TRANSFORM_ERRNO_FILE_FORMAT */
/* Illegal usage of the library. */
/* #define	TRANSFORM_ERRNO_PROGRAMMER_ERROR */
/* Unknown or unclassified error. */
/* #define	TRANSFORM_ERRNO_MISC */

/*
 * Callbacks are invoked to automatically read/skip/write/open/close the
 * transform. You can provide your own for complex tasks (like breaking
 * transforms across multiple tapes) or use standard ones built into the
 * library.
 */

struct transform_read_filter;

int64_t transform_read_filter_consume(struct transform_read_filter *, int64_t);
const void *transform_read_filter_ahead(struct transform_read_filter *,
	size_t, ssize_t *);
void transform_read_filter_free(struct transform_read_filter *);

/* Returns pointer and size of next block of data from transform. */
typedef __LA_SSIZE_T	transform_read_callback(struct transform *,
	void *_client_data, struct transform_read_filter *, const void **_buffer);

/* Skips at most request bytes from transform and returns the skipped amount */
/* Libtransform 3.0 uses int64_t here, which is actually guaranteed to be
 * 64 bits on every platform. */
typedef __LA_INT64_T	transform_skip_callback(struct transform *,
			    void *_client_data, __LA_INT64_T request);

/* Returns size actually written, zero on EOF, -1 on error. */
typedef __LA_SSIZE_T	transform_write_callback(struct transform *,
			    void *_client_data,
			    const void *_buffer, size_t _length);

typedef int	transform_open_callback(struct transform *, void *_client_data);

typedef int	transform_close_callback(struct transform *, void *_client_data);

typedef int transform_fd_visitor(struct transform *,
	int fd, void *visitor_data);

/*
 * Codes to identify various stream filters.
 */
#define	TRANSFORM_FILTER_NONE	0
#define	TRANSFORM_FILTER_GZIP	1
#define	TRANSFORM_FILTER_BZIP2	2
#define	TRANSFORM_FILTER_COMPRESS	3
#define	TRANSFORM_FILTER_PROGRAM	4
#define	TRANSFORM_FILTER_LZMA	5
#define	TRANSFORM_FILTER_XZ	6
#define	TRANSFORM_FILTER_UU	7
#define	TRANSFORM_FILTER_RPM	8
#define	TRANSFORM_FILTER_LZIP	9

/*-
 * Basic outline for reading an transform:
 *   1) Ask transform_read_new for an transform reader object.
 *   2) Update any global properties as appropriate.
 *      In particular, you'll certainly want to call appropriate
 *      transform_read_support_XXX functions.
 *   3) Call transform_read_open_XXX to open the transform
 *   4) Repeatedly call transform_read_next_header to get information about
 *      successive transform entries.  Call transform_read_data to extract
 *      data for entries of interest.
 *   5) Call transform_read_finish to end processing.
 */
__LA_DECL struct transform	*transform_read_new(void);

/*
 * The transform_read_support_XXX calls enable auto-detect for this
 * transform handle.  They also link in the necessary support code.
 * For example, if you don't want bzlib linked in, don't invoke
 * support_compression_bzip2().  The "all" functions provide the
 * obvious shorthand.
 */
/* TODO: Rename 'compression' here to 'filter' for libtransform 3.0, deprecate
 * the old names. */

__LA_DECL int transform_read_support_compression_all(struct transform *);
__LA_DECL int transform_read_support_compression_bzip2(struct transform *);
__LA_DECL int transform_read_support_compression_compress(struct transform *);
__LA_DECL int transform_read_support_compression_gzip(struct transform *);
__LA_DECL int transform_read_support_compression_lzip(struct transform *);
__LA_DECL int transform_read_support_compression_lzma(struct transform *);
__LA_DECL int transform_read_support_compression_none(struct transform *);
__LA_DECL int transform_read_support_compression_program(struct transform *,
		     const char *command);
__LA_DECL int transform_read_support_compression_program_signature
		(struct transform *, const char *,
				    const void * /* match */, size_t);

__LA_DECL int transform_read_support_compression_rpm(struct transform *);
__LA_DECL int transform_read_support_compression_uu(struct transform *);
__LA_DECL int transform_read_support_compression_xz(struct transform *);


/* Open the transform using callbacks for transform I/O. */
__LA_DECL int transform_read_open(struct transform *, void *_client_data,
		     transform_open_callback *, transform_read_callback *,
		     transform_skip_callback *, transform_close_callback *);

/*
 * A variety of shortcuts that invoke transform_read_open() with
 * canned callbacks suitable for common situations.  The ones that
 * accept a block size handle tape blocking correctly.
 */
/* Use this if you know the filename.  Note: NULL indicates stdin. */
__LA_DECL int transform_read_open_filename(struct transform *,
		     const char *_filename, size_t _block_size);
__LA_DECL int transform_read_open_filename_fd(struct transform *a, const char *filename,
    size_t block_size, int fd);
/* transform_read_open_file() is a deprecated synonym for ..._open_filename(). */
__LA_DECL int transform_read_open_file(struct transform *,
		     const char *_filename, size_t _block_size);
/* Read an transform that's stored in memory. */
__LA_DECL int transform_read_open_memory(struct transform *,
		     void * buff, size_t size);
/* A more involved version that is only used for internal testing. */
__LA_DECL int transform_read_open_memory2(struct transform *a, void *buff,
		     size_t size, size_t read_size);
/* Read an transform that's already open, using the file descriptor. */
__LA_DECL int transform_read_open_fd(struct transform *, int _fd,
		     size_t _block_size);
/* Read an transform that's already open, using a FILE *. */
/* Note: DO NOT use this with tape drives. */
__LA_DECL int transform_read_open_FILE(struct transform *, FILE *_file);

__LA_DECL const void *transform_read_ahead(struct transform *, size_t, ssize_t *);
__LA_DECL int64_t transform_read_consume(struct transform *, int64_t);
__LA_DECL int64_t transform_read_bytes_consumed(struct transform *);

/*
 * Retrieve the byte offset in UNCOMPRESSED data where last-read
 * header started.
 */
__LA_DECL __LA_INT64_T		 transform_read_header_position(struct transform *);

/* Read data from the body of an entry.  Similar to read(2). */
__LA_DECL __LA_SSIZE_T		 transform_read_data(struct transform *,
				    void *, size_t);

/*-
 * Some convenience functions that are built on transform_read_data:
 *  'skip': skips entire entry
 *  'into_buffer': writes data into memory buffer that you provide
 *  'into_fd': writes data to specified filedes
 */
__LA_DECL int transform_read_data_into_fd(struct transform *, int fd);

/*
 * Set read options.
 */
/* Apply option string to the filter only. */
__LA_DECL int transform_read_set_filter_options(struct transform *,
			    const char *);
__LA_DECL int transform_write_set_filter_option(struct transform *, const char *,
    const char *, const char *);

/*-
 * Convenience function to recreate the current entry (whose header
 * has just been read) on disk.
 *
 * This does quite a bit more than just copy data to disk. It also:
 *  - Creates intermediate directories as required.
 *  - Manages directory permissions:  non-writable directories will
 *    be initially created with write permission enabled; when the
 *    transform is closed, dir permissions are edited to the values specified
 *    in the transform.
 *  - Checks hardlinks:  hardlinks will not be extracted unless the
 *    linked-to file was also extracted within the same session. (TODO)
 */

/* The "flags" argument selects optional behavior, 'OR' the flags you want. */

/* Default: Do not try to set owner/group. */
#define	TRANSFORM_EXTRACT_OWNER			(0x0001)
/* Default: Do obey umask, do not restore SUID/SGID/SVTX bits. */
#define	TRANSFORM_EXTRACT_PERM			(0x0002)
/* Default: Do not restore mtime/atime. */
#define	TRANSFORM_EXTRACT_TIME			(0x0004)
/* Default: Replace existing files. */
#define	TRANSFORM_EXTRACT_NO_OVERWRITE 		(0x0008)
/* Default: Try create first, unlink only if create fails with EEXIST. */
#define	TRANSFORM_EXTRACT_UNLINK			(0x0010)
/* Default: Do not restore fflags. */
#define	TRANSFORM_EXTRACT_FFLAGS			(0x0040)
/* Default: Do not try to guard against extracts redirected by symlinks. */
/* Note: With TRANSFORM_EXTRACT_UNLINK, will remove any intermediate symlink. */
#define	TRANSFORM_EXTRACT_SECURE_SYMLINKS		(0x0100)
/* Default: Do not reject entries with '..' as path elements. */
#define	TRANSFORM_EXTRACT_SECURE_NODOTDOT		(0x0200)
/* Default: Create parent directories as needed. */
#define	TRANSFORM_EXTRACT_NO_AUTODIR		(0x0400)
/* Default: Overwrite files, even if one on disk is newer. */
#define	TRANSFORM_EXTRACT_NO_OVERWRITE_NEWER	(0x0800)
/* Detect blocks of 0 and write holes instead. */
#define	TRANSFORM_EXTRACT_SPARSE			(0x1000)

/* Close the file and release most resources. */
__LA_DECL int		 transform_read_close(struct transform *);
/* Release all resources and destroy the object. */
/* Note that transform_read_free will call transform_read_close for you. */
__LA_DECL int		 transform_read_free(struct transform *);
/* reset all filters currently associated */
__LA_DECL int 		 transform_write_reset_filters(struct transform *);

/*-
 * To create an transform:
 *   1) Ask transform_write_new for a transform writer object.
 *   2) Set any global properties.  In particular, you should set
 *      the compression and format to use.
 *   3) Call transform_write_open to open the file (most people
 *       will use transform_write_open_file or transform_write_open_fd,
 *       which provide convenient canned I/O callbacks for you).
 *   4) write data to it via transform_write_output
 *   5) transform_write_close to close the output
 *   6) transform_write_free to cleanup the writer and release resources
 */
__LA_DECL struct transform	*transform_write_new(void);
__LA_DECL int transform_write_output(struct transform *, const void *, size_t);
__LA_DECL int transform_write_set_bytes_per_block(struct transform *,
		     int bytes_per_block);
__LA_DECL int transform_write_get_bytes_per_block(struct transform *);
/* XXX This is badly misnamed; suggestions appreciated. XXX */
__LA_DECL int transform_write_set_bytes_in_last_block(struct transform *,
		     int bytes_in_last_block);
__LA_DECL int transform_write_get_bytes_in_last_block(struct transform *);

__LA_DECL int transform_write_add_filter_bzip2(struct transform *);
__LA_DECL int transform_write_add_filter_compress(struct transform *);
__LA_DECL int transform_write_add_filter_gzip(struct transform *);
__LA_DECL int transform_write_add_filter_lzma(struct transform *);
__LA_DECL int transform_write_add_filter_lzip(struct transform *);
__LA_DECL int transform_write_add_filter_none(struct transform *);
__LA_DECL int transform_write_add_filter_program(struct transform *,
		     const char *cmd);
__LA_DECL int transform_write_add_filter_xz(struct transform *);


__LA_DECL int transform_write_open(struct transform *, void *,
		     transform_open_callback *, transform_write_callback *,
		     transform_close_callback *);
__LA_DECL int transform_write_open_fd(struct transform *, int _fd);
__LA_DECL int transform_write_open_filename(struct transform *, const char *_file);
/* A deprecated synonym for transform_write_open_filename() */
__LA_DECL int transform_write_open_file(struct transform *, const char *_file);
__LA_DECL int transform_write_open_FILE(struct transform *, FILE *);

/* _buffSize is the size of the buffer, _used refers to a variable that
 * will be updated after each write into the buffer. */
__LA_DECL int transform_write_open_memory(struct transform *,
			void *_buffer, size_t _buffSize, size_t *_used);

__LA_DECL int		 transform_write_close(struct transform *);
/* This can fail if the transform wasn't already closed, in which case
 * transform_write_free() will implicitly call transform_write_close(). */
__LA_DECL int		 transform_write_free(struct transform *);

/*
 * Set write options.  Note that there's really no reason to use
 * anything but transform_write_set_options().  The others should probably
 * all be deprecated and eventually removed.
 */
/* Apply option string to both the format and all filters. */
__LA_DECL int		transform_write_set_options(struct transform *_a,
			    const char *s);
/* Apply option string to all matching filters. */
__LA_DECL int		transform_write_set_filter_options(struct transform *_a,
			    const char *s);

__LA_DECL int transform_is_read(struct transform *);
__LA_DECL int transform_is_write(struct transform *);

/*
 * Accessor functions to read/set various information in
 * the struct transform object:
 */

/* Number of filters in the current filter pipeline. */
/* Filter #0 is the one closest to the format, -1 is a synonym for the
 * last filter, which is always the pseudo-filter that wraps the
 * client callbacks. */
__LA_DECL int		 transform_filter_count(struct transform *);
__LA_DECL __LA_INT64_T	 transform_filter_bytes(struct transform *, int);
__LA_DECL int		 transform_filter_code(struct transform *, int);
__LA_DECL const char *	 transform_filter_name(struct transform *, int);
__LA_DECL int        transform_visit_fds(struct transform *,
	transform_fd_visitor *, const void *);

__LA_DECL int		 transform_errno(struct transform *);
__LA_DECL const char	*transform_error_string(struct transform *);
__LA_DECL void		 transform_clear_error(struct transform *);
__LA_DECL void		 transform_set_error(struct transform *, int _err,
			    const char *fmt, ...) __LA_PRINTF(3, 4);
__LA_DECL void		 transform_copy_error(struct transform *dest,
			    struct transform *src);

struct transform_read_bidder;
struct transform_read_filter;
/*
 * read bidders
 */

typedef int transform_read_bidder_bid_method(const void *bidder_data,
	struct transform_read_filter *source);
typedef int transform_read_bidder_set_option(const void *bidder_data,
	const char *key, const char *value);
typedef struct transform_read_filter *
	transform_read_bidder_create_filter(struct transform *,
	const void *bidder_data);
typedef int transform_read_bidder_free(const void *bidder_data);

__LA_DECL struct transform_read_bidder *
	transform_read_bidder_add(struct transform *,
	const void *data, const char *,
	transform_read_bidder_bid_method *,
	transform_read_bidder_create_filter *,
	transform_read_bidder_free *,
	transform_read_bidder_set_option *);

typedef int transform_visit_fds_callback(struct transform *,
	const void *data, transform_fd_visitor *visitor, const void *visitor_data);

#define transform_read_filter_visit_fds_callback transform_visit_fds_callback
#define transform_read_filter_close_callback transform_close_callback
#define transform_read_filter_skip_callback transform_skip_callback
#define transform_read_filter_read_callback transform_read_callback

/* Returns size actually written, zero on EOF, -1 on error. */

                        
__LA_DECL int transform_read_filter_add(struct transform *,
	struct transform_read_filter *);

__LA_DECL struct transform_read_filter *
	transform_read_filter_new(
	const void *data, const char *filter_name, int code,
	transform_read_filter_read_callback *,
	transform_read_filter_skip_callback *,
	transform_read_filter_close_callback *,
	transform_read_filter_visit_fds_callback *);

                    

#ifdef __cplusplus
}
#endif

/* These are meaningless outside of this header. */
#undef __LA_DECL
#undef __LA_GID_T
#undef __LA_UID_T

/* These need to remain defined because they're used in the
 * callback type definitions.  XXX Fix this.  This is ugly. XXX */
/* #undef __LA_INT64_T */
/* #undef __LA_SSIZE_T */

#endif /* !TRANSFORM_H_INCLUDED */
