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
 * Basic resizable/reusable string support a la Java's "StringBuffer."
 *
 * Unlike sbuf(9), the buffers here are fully reusable and track the
 * length throughout.
 *
 * Note that all visible symbols here begin with "__transform" as they
 * are internal symbols not intended for anyone outside of this library
 * to see or use.
 */

struct transform_string {
	char	*s;  /* Pointer to the storage */
	size_t	 length; /* Length of 's' */
	size_t	 buffer_length; /* Length of malloc-ed storage */
};

struct transform_wstring {
	wchar_t	*s;  /* Pointer to the storage */
	size_t	 length; /* Length of 's' in characters */
	size_t	 buffer_length; /* Length of malloc-ed storage */
};

/* Initialize an transform_string object on the stack or elsewhere. */
#define	transform_string_init(a)	\
	do { (a)->s = NULL; (a)->length = 0; (a)->buffer_length = 0; } while(0)

/* Append a C char to an transform_string, resizing as necessary. */
struct transform_string *
__transform_strappend_char(struct transform_string *, char);
#define	transform_strappend_char __transform_strappend_char
struct transform_wstring *
__transform_wstrappend_wchar(struct transform_wstring *, wchar_t);
#define	transform_wstrappend_wchar __transform_wstrappend_wchar

/* Convert a wide-char string to UTF-8 and append the result. */
struct transform_string *
__transform_strappend_w_utf8(struct transform_string *, const wchar_t *);
#define	transform_strappend_w_utf8	__transform_strappend_w_utf8

/* Convert a wide-char string to current locale and append the result. */
/* Returns NULL if conversion fails. */
struct transform_string *
__transform_strappend_w_mbs(struct transform_string *, const wchar_t *);
#define	transform_strappend_w_mbs	__transform_strappend_w_mbs

/* Basic append operation. */
struct transform_string *
__transform_string_append(struct transform_string *as, const char *p, size_t s);
struct transform_wstring *
__transform_wstring_append(struct transform_wstring *as, const wchar_t *p, size_t s);

/* Copy one transform_string to another */
void
__transform_string_copy(struct transform_string *dest, struct transform_string *src);
#define transform_string_copy(dest, src)		\
	__transform_string_copy((dest), (src))
void
__transform_wstring_copy(struct transform_wstring *dest, struct transform_wstring *src);
#define transform_wstring_copy(dest, src)		\
	__transform_wstring_copy((dest), (src))

/* Concatenate one transform_string to another */
void
__transform_string_concat(struct transform_string *dest, struct transform_string *src);
#define transform_string_concat(dest, src) \
	__transform_string_concat(dest, src)

/* Ensure that the underlying buffer is at least as large as the request. */
struct transform_string *
__transform_string_ensure(struct transform_string *, size_t);
#define	transform_string_ensure __transform_string_ensure

struct transform_wstring *
__transform_wstring_ensure(struct transform_wstring *, size_t);
#define	transform_wstring_ensure __transform_wstring_ensure

/* Append C string, which may lack trailing \0. */
/* The source is declared void * here because this gets used with
 * "signed char *", "unsigned char *" and "char *" arguments.
 * Declaring it "char *" as with some of the other functions just
 * leads to a lot of extra casts. */
struct transform_string *
__transform_strncat(struct transform_string *, const void *, size_t);
#define	transform_strncat  __transform_strncat
struct transform_wstring *
__transform_wstrncat(struct transform_wstring *, const void *, size_t);
#define	transform_wstrncat  __transform_wstrncat

/* Append a C string to an transform_string, resizing as necessary. */
#define	transform_strcat(as,p) __transform_string_append((as),(p),strlen(p))
#define	transform_wstrcat(as,p) __transform_wstring_append((as),(p),wcslen(p))

/* Copy a C string to an transform_string, resizing as necessary. */
#define	transform_strcpy(as,p) \
	transform_strncpy((as), (p), ((p) == NULL ? 0 : strlen(p)))
#define	transform_wstrcpy(as,p) \
	transform_wstrncpy((as), (p), ((p) == NULL ? 0 : wcslen(p)))

/* Copy a C string to an transform_string with limit, resizing as necessary. */
#define	transform_strncpy(as,p,l) \
	((as)->length=0, transform_strncat((as), (p), (l)))
#define	transform_wstrncpy(as,p,l) \
	((as)->length = 0, __transform_wstring_append((as), (p), (l)))

/* Return length of string. */
#define	transform_strlen(a) ((a)->length)

/* Set string length to zero. */
#define	transform_string_empty(a) ((a)->length = 0)
#define	transform_wstring_empty(a) ((a)->length = 0)

/* Release any allocated storage resources. */
void	__transform_string_free(struct transform_string *);
#define	transform_string_free  __transform_string_free
void	__transform_wstring_free(struct transform_wstring *);
#define	transform_wstring_free  __transform_wstring_free

/* Like 'vsprintf', but resizes the underlying string as necessary. */
void	__transform_string_vsprintf(struct transform_string *, const char *,
	    va_list) __LA_PRINTF(2, 0);
#define	transform_string_vsprintf	__transform_string_vsprintf

void	__transform_string_sprintf(struct transform_string *, const char *, ...)
	    __LA_PRINTF(2, 3);
#define	transform_string_sprintf	__transform_string_sprintf

/* Translates from UTF8 in src to Unicode in dest. */
/* Returns non-zero if conversion failed in any way. */
int __transform_wstrappend_utf8(struct transform_wstring *dest,
			      struct transform_string *src);

/* Translates from MBS in src to Unicode in dest. */
/* Returns non-zero if conversion failed in any way. */
int __transform_wstrappend_mbs(struct transform_wstring *dest,
			      struct transform_string *src);

#endif
