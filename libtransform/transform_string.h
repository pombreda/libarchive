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
 * $FreeBSD: head/lib/libtransform/transform_string.h 201092 2009-12-28 02:26:06Z kientzle $
 *
 */

#ifndef __LIBTRANSFORM_BUILD
#error This header is only to be used internally to libtransform.
#endif

#ifndef TRANSFORM_STRING_H_INCLUDED
#define	TRANSFORM_STRING_H_INCLUDED

#include <stdarg.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>  /* required for wchar_t on some systems */
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif

#include "transform.h"

/*
 * Basic resizable/reusable string support similar to Java's "StringBuffer."
 *
 * Unlike sbuf(9), the buffers here are fully reusable and track the
 * length throughout.
 */

struct transform_string {
	char	*s;  /* Pointer to the storage */
	size_t	 length; /* Length of 's' in characters */
	size_t	 buffer_length; /* Length of malloc-ed storage in bytes. */
};

struct transform_wstring {
	wchar_t	*s;  /* Pointer to the storage */
	size_t	 length; /* Length of 's' in characters */
	size_t	 buffer_length; /* Length of malloc-ed storage in bytes. */
};

/* Initialize an transform_string object on the stack or elsewhere. */
#define	transform_string_init(a)	\
	do { (a)->s = NULL; (a)->length = 0; (a)->buffer_length = 0; } while(0)

/* Append a C char to an transform_string, resizing as necessary. */
struct transform_string *
transform_strappend_char(struct transform_string *, char);

/* Ditto for a wchar_t and an transform_wstring. */
struct transform_wstring *
transform_wstrappend_wchar(struct transform_wstring *, wchar_t);

/* Convert a Unicode string to UTF-8 and append the result. */
struct transform_string *
transform_strappend_w_utf8(struct transform_string *, const wchar_t *);

/* Convert a Unicode string to current locale and append the result. */
/* Returns NULL if conversion fails. */
struct transform_string *
transform_strappend_w_mbs(struct transform_string *, const wchar_t *);

/* Copy one transform_string to another */
#define	transform_string_copy(dest, src) \
	((dest)->length = 0, transform_string_concat((dest), (src)))
#define	transform_wstring_copy(dest, src) \
	((dest)->length = 0, transform_wstring_concat((dest), (src)))

/* Concatenate one transform_string to another */
void transform_string_concat(struct transform_string *dest, struct transform_string *src);
void transform_wstring_concat(struct transform_wstring *dest, struct transform_wstring *src);

/* Ensure that the underlying buffer is at least as large as the request. */
struct transform_string *
transform_string_ensure(struct transform_string *, size_t);
struct transform_wstring *
transform_wstring_ensure(struct transform_wstring *, size_t);

/* Append C string, which may lack trailing \0. */
/* The source is declared void * here because this gets used with
 * "signed char *", "unsigned char *" and "char *" arguments.
 * Declaring it "char *" as with some of the other functions just
 * leads to a lot of extra casts. */
struct transform_string *
transform_strncat(struct transform_string *, const void *, size_t);
struct transform_wstring *
transform_wstrncat(struct transform_wstring *, const wchar_t *, size_t);

/* Append a C string to an transform_string, resizing as necessary. */
struct transform_string *
transform_strcat(struct transform_string *, const void *);
struct transform_wstring *
transform_wstrcat(struct transform_wstring *, const wchar_t *);

/* Copy a C string to an transform_string, resizing as necessary. */
#define	transform_strcpy(as,p) \
	transform_strncpy((as), (p), ((p) == NULL ? 0 : strlen(p)))
#define	transform_wstrcpy(as,p) \
	transform_wstrncpy((as), (p), ((p) == NULL ? 0 : wcslen(p)))

/* Copy a C string to an transform_string with limit, resizing as necessary. */
#define	transform_strncpy(as,p,l) \
	((as)->length=0, transform_strncat((as), (p), (l)))
#define	transform_wstrncpy(as,p,l) \
	((as)->length = 0, transform_wstrncat((as), (p), (l)))

/* Return length of string. */
#define	transform_strlen(a) ((a)->length)

/* Set string length to zero. */
#define	transform_string_empty(a) ((a)->length = 0)
#define	transform_wstring_empty(a) ((a)->length = 0)

/* Release any allocated storage resources. */
void	transform_string_free(struct transform_string *);
void	transform_wstring_free(struct transform_wstring *);

/* Like 'vsprintf', but resizes the underlying string as necessary. */
/* Note: This only implements a small subset of standard printf functionality. */
void	transform_string_vsprintf(struct transform_string *, const char *,
	    va_list) __LA_PRINTF(2, 0);
void	transform_string_sprintf(struct transform_string *, const char *, ...)
	    __LA_PRINTF(2, 3);

/* Translates from UTF8 in src to Unicode in dest. */
/* Returns non-zero if conversion failed in any way. */
int transform_wstrappend_utf8(struct transform_wstring *dest,
			      struct transform_string *src);

/* Translates from MBS in src to Unicode in dest. */
/* Returns non-zero if conversion failed in any way. */
int transform_wstrcpy_mbs(struct transform_wstring *dest,
			      struct transform_string *src);


/* A "multistring" can hold Unicode, UTF8, or MBS versions of
 * the string.  If you set and read the same version, no translation
 * is done.  If you set and read different versions, the library
 * will attempt to transparently convert.
 */
struct transform_mstring {
	struct transform_string aes_mbs;
	struct transform_string aes_utf8;
	struct transform_wstring aes_wcs;
	/* Bitmap of which of the above are valid.  Because we're lazy
	 * about malloc-ing and reusing the underlying storage, we
	 * can't rely on NULL pointers to indicate whether a string
	 * has been set. */
	int aes_set;
#define	AES_SET_MBS 1
#define	AES_SET_UTF8 2
#define	AES_SET_WCS 4
};

void	transform_mstring_clean(struct transform_mstring *);
void	transform_mstring_copy(struct transform_mstring *dest, struct transform_mstring *src);
const char *	transform_mstring_get_mbs(struct transform_mstring *);
const wchar_t *	transform_mstring_get_wcs(struct transform_mstring *);
int	transform_mstring_copy_mbs(struct transform_mstring *, const char *mbs);
int	transform_mstring_copy_wcs(struct transform_mstring *, const wchar_t *wcs);
int	transform_mstring_copy_wcs_len(struct transform_mstring *, const wchar_t *wcs, size_t);
int     transform_mstring_update_utf8(struct transform_mstring *aes, const char *utf8);


#endif
