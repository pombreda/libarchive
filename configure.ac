dnl Process this file with autoconf to produce a configure script.

dnl First, define all of the version numbers up front.
dnl In particular, this allows the version macro to be used in AC_INIT

dnl These first two version numbers are updated automatically on each release.
m4_define([LIBARCHIVE_VERSION_S],[3.0.200a])
m4_define([LIBARCHIVE_VERSION_N],[3000200])

dnl bsdtar and bsdcpio versioning tracks libarchive
m4_define([BSDTAR_VERSION_S],LIBARCHIVE_VERSION_S())
m4_define([BSDCPIO_VERSION_S],LIBARCHIVE_VERSION_S())

AC_PREREQ(2.65)

#
# Now starts the "real" configure script.
#

AC_INIT([libarchive],LIBARCHIVE_VERSION_S(),[kientzle@FreeBSD.org])
# Make sure the srcdir contains "libarchive" directory
AC_CONFIG_SRCDIR([libarchive])
# Use auxiliary subscripts from this subdirectory (cleans up root)
AC_CONFIG_AUX_DIR([build/autoconf])
# M4 scripts
AC_CONFIG_MACRO_DIR([build/autoconf])
# Must follow AC_CONFIG macros above...
AM_INIT_AUTOMAKE()

# Libtool's "interface version" can be computed from the libarchive version.

# Libtool interface version bumps on any API change, so increments
# whenever libarchive minor version does.
ARCHIVE_MINOR=$(( (LIBARCHIVE_VERSION_N() / 1000) % 1000 ))
# Libarchive 2.7 == libtool interface 9 = 2 + 7
# Libarchive 2.8 == libtool interface 10 = 2 + 8
# Libarchive 2.9 == libtool interface 11 = 2 + 8
# Libarchive 3.0 == libtool interface 12
# Libarchive 3.x == libtool interface 12 + x
ARCHIVE_INTERFACE=`echo $((12 + ${ARCHIVE_MINOR}))`
# Libarchive revision is bumped on any source change === libtool revision
ARCHIVE_REVISION=$(( LIBARCHIVE_VERSION_N() % 1000 ))
# Libarchive minor is bumped on any interface addition === libtool age
ARCHIVE_LIBTOOL_VERSION=$ARCHIVE_INTERFACE:$ARCHIVE_REVISION:$ARCHIVE_MINOR

# Stick the version numbers into config.h
AC_DEFINE([LIBARCHIVE_VERSION_STRING],"LIBARCHIVE_VERSION_S()",
	[Version number of libarchive])
AC_DEFINE_UNQUOTED([LIBARCHIVE_VERSION_NUMBER],"LIBARCHIVE_VERSION_N()",
	[Version number of libarchive as a single integer])
AC_DEFINE([BSDCPIO_VERSION_STRING],"BSDCPIO_VERSION_S()",
	[Version number of bsdcpio])
AC_DEFINE([BSDTAR_VERSION_STRING],"BSDTAR_VERSION_S()",
	[Version number of bsdtar])

# The shell variables here must be the same as the AC_SUBST() variables
# below, but the shell variable names apparently cannot be the same as
# the m4 macro names above.  Why?  Ask autoconf.
BSDCPIO_VERSION_STRING=BSDCPIO_VERSION_S()
BSDTAR_VERSION_STRING=BSDTAR_VERSION_S()
LIBARCHIVE_VERSION_STRING=LIBARCHIVE_VERSION_S()
LIBARCHIVE_VERSION_NUMBER=LIBARCHIVE_VERSION_N()

# Substitute the above version numbers into the various files below.
# Yes, I believe this is the fourth time we define what are essentially
# the same symbols.  Why? Ask autoconf.
AC_SUBST(ARCHIVE_LIBTOOL_VERSION)
AC_SUBST(BSDCPIO_VERSION_STRING)
AC_SUBST(BSDTAR_VERSION_STRING)
AC_SUBST(LIBARCHIVE_VERSION_STRING)
AC_SUBST(LIBARCHIVE_VERSION_NUMBER)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([build/pkgconfig/libarchive.pc])

# Check for host type
AC_CANONICAL_HOST

dnl Compilation on mingw and Cygwin needs special Makefile rules
inc_windows_files=no
inc_cygwin_files=no
case "$host_os" in
  *mingw* ) inc_windows_files=yes ;;
  *cygwin*) inc_cygwin_files=yes ;;
esac
AM_CONDITIONAL([INC_WINDOWS_FILES], [test $inc_windows_files = yes])
AM_CONDITIONAL([INC_CYGWIN_FILES], [test $inc_cygwin_files = yes])

dnl Defines that are required for specific platforms (e.g. -D_POSIX_SOURCE, etc)
PLATFORMCPPFLAGS=
case "$host_os" in
  *mingw* ) PLATFORMCPPFLAGS=-D__USE_MINGW_ANSI_STDIO ;;
esac
AC_SUBST(PLATFORMCPPFLAGS)

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_USE_SYSTEM_EXTENSIONS
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AC_CHECK_TOOL([STRIP],[strip])

#
# Options for building bsdtar.
#
# Default is to build bsdtar, but allow people to override that.
#
AC_ARG_ENABLE([bsdtar],
	[AS_HELP_STRING([--enable-bsdtar], [enable build of bsdtar (default)])
	AS_HELP_STRING([--enable-bsdtar=static], [force static build of bsdtar])
	AS_HELP_STRING([--enable-bsdtar=shared], [force dynamic build of bsdtar])
AS_HELP_STRING([--disable-bsdtar], [disable build of bsdtar])],
	[], [enable_bsdtar=yes])

case "$enable_bsdtar" in
yes)
	if test "$enable_static" = "no"; then
		static_bsdtar=no
	else
		static_bsdtar=yes
	fi
	build_bsdtar=yes
	;;
dynamic|shared)
	if test "$enable_shared" = "no"; then
		AC_MSG_FAILURE([Shared linking of bsdtar requires shared libarchive])
	fi
	build_bsdtar=yes
	static_bsdtar=no
	;;
static)
	build_bsdtar=yes
	static_bsdtar=yes
	;;
no)
	build_bsdtar=no
	static_bsdtar=no
	;;
*)
	AC_MSG_FAILURE([Unsupported value for --enable-bsdtar])
	;;
esac

AM_CONDITIONAL([BUILD_BSDTAR], [ test "$build_bsdtar" = yes ])
AM_CONDITIONAL([STATIC_BSDTAR], [ test "$static_bsdtar" = yes ])

#
# Options for building bsdcpio.
#
# Default is not to build bsdcpio, but that can be overridden.
#
AC_ARG_ENABLE([bsdcpio],
	[AS_HELP_STRING([--enable-bsdcpio], [enable build of bsdcpio (default)])
	AS_HELP_STRING([--enable-bsdcpio=static], [static build of bsdcpio])
	AS_HELP_STRING([--enable-bsdcpio=shared], [dynamic build of bsdcpio])
AS_HELP_STRING([--disable-bsdcpio], [disable build of bsdcpio])],
	[], [enable_bsdcpio=yes])

case "$enable_bsdcpio" in
yes)
	if test "$enable_static" = "no"; then
	   static_bsdcpio=no
	else
	   static_bsdcpio=yes
        fi
	build_bsdcpio=yes
	;;
dynamic|shared)
	if test "$enabled_shared" = "no"; then
	   AC_MSG_FAILURE([Shared linking of bsdcpio requires shared libarchive])
	fi
	build_bsdcpio=yes
	;;
static)
	build_bsdcpio=yes
	static_bsdcpio=yes
	;;
no)
	build_bsdcpio=no
	static_bsdcpio=no
	;;
*)
	AC_MSG_FAILURE([Unsupported value for --enable-bsdcpio])
	;;
esac

AM_CONDITIONAL([BUILD_BSDCPIO], [ test "$build_bsdcpio" = yes ])
AM_CONDITIONAL([STATIC_BSDCPIO], [ test "$static_bsdcpio" = yes ])

# Set up defines needed before including any headers
case $host in
  *mingw* | *cygwin* )
  AC_DEFINE([_WIN32_WINNT], 0x0500, [Define to '0x0500' for Windows 2000 APIs.])
  AC_DEFINE([WINVER], 0x0500, [Define to '0x0500' for Windows 2000 APIs.])
  ;;
esac

# We need CoreServices on Mac OS.
case $host in
  *darwin* )
  LIBS="${LIBS} -framework CoreServices"
  ;;
esac

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([acl/libacl.h attr/xattr.h copyfile.h ctype.h])
AC_CHECK_HEADERS([errno.h ext2fs/ext2_fs.h fcntl.h grp.h])

AC_CACHE_CHECK([whether EXT2_IOC_GETFLAGS is usable],
    [ac_cv_have_decl_EXT2_IOC_GETFLAGS],
    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([@%:@include <sys/ioctl.h>
@%:@include <ext2fs/ext2_fs.h>],
                                   [int x = EXT2_IOC_GETFLAGS])],
                  [AS_VAR_SET([ac_cv_have_decl_EXT2_IOC_GETFLAGS], [yes])],
                  [AS_VAR_SET([ac_cv_have_decl_EXT2_IOC_GETFLAGS], [no])])])

AS_VAR_IF([ac_cv_have_decl_EXT2_IOC_GETFLAGS], [yes],
    [AC_DEFINE_UNQUOTED([HAVE_WORKING_EXT2_IOC_GETFLAGS], [1],
                    [Define to 1 if you have a working EXT2_IOC_GETFLAGS])])

AC_CHECK_HEADERS([inttypes.h io.h langinfo.h limits.h])
AC_CHECK_HEADERS([linux/fiemap.h linux/fs.h linux/magic.h])
AC_CHECK_HEADERS([locale.h paths.h poll.h pwd.h regex.h signal.h stdarg.h])
AC_CHECK_HEADERS([stdint.h stdlib.h string.h])
AC_CHECK_HEADERS([sys/acl.h sys/cdefs.h sys/extattr.h sys/ioctl.h])
AC_CHECK_HEADERS([sys/mkdev.h sys/mount.h])
AC_CHECK_HEADERS([sys/param.h sys/poll.h sys/select.h sys/statfs.h sys/statvfs.h])
AC_CHECK_HEADERS([sys/time.h sys/utime.h sys/utsname.h sys/vfs.h])
AC_CHECK_HEADERS([time.h unistd.h utime.h wchar.h wctype.h])
AC_CHECK_HEADERS([windows.h])
# check windows.h first; the other headers require it.
AC_CHECK_HEADERS([wincrypt.h winioctl.h],[],[],
[[#ifdef HAVE_WINDOWS_H
# include <windows.h>
#endif
]])

# Checks for libraries.
AC_ARG_WITH([zlib],
  AS_HELP_STRING([--without-zlib], [Don't build support for gzip through zlib]))

if test "x$with_zlib" != "xno"; then
  AC_CHECK_HEADERS([zlib.h])
  AC_CHECK_LIB(z,inflate)
fi

AC_ARG_WITH([bz2lib],
  AS_HELP_STRING([--without-bz2lib], [Don't build support for bzip2 through bz2lib]))

if test "x$with_bz2lib" != "xno"; then
  AC_CHECK_HEADERS([bzlib.h])
  AC_CHECK_LIB(bz2,BZ2_bzDecompressInit)
fi

AC_ARG_WITH([lzmadec],
  AS_HELP_STRING([--without-lzmadec], [Don't build support for lzma through lzmadec]))

if test "x$with_lzmadec" != "xno"; then
  AC_CHECK_HEADERS([lzmadec.h])
  AC_CHECK_LIB(lzmadec,lzmadec_decode)
fi

AC_ARG_WITH([iconv],
  AS_HELP_STRING([--without-iconv], [Don't try to link against iconv]))

if test "x$with_iconv" != "xno"; then
  AC_CHECK_HEADERS([iconv.h],[],[],[#include <stdlib.h>])
  AM_ICONV
  if test "x$am_cv_func_iconv" = "xyes"; then
    AC_CHECK_HEADERS([localcharset.h])
    am_save_LIBS="$LIBS"
    LIBS="${LIBS} ${LIBICONV}"
    AC_CHECK_FUNCS([locale_charset])
    LIBS="${am_save_LIBS}"
    if test "x$ac_cv_func_locale_charset" != "xyes"; then
      # If locale_charset() is not in libiconv, we have to find libcharset. 
      AC_CHECK_LIB(charset,locale_charset)
    fi
  fi
fi

AC_ARG_WITH([lzma],
  AS_HELP_STRING([--without-lzma], [Don't build support for xz through lzma]))

if test "x$with_lzma" != "xno"; then
  AC_CHECK_HEADERS([lzma.h])
  AC_CHECK_LIB(lzma,lzma_stream_decoder)
fi

AC_ARG_WITH([nettle],
  AS_HELP_STRING([--without-nettle], [Don't build with crypto support from Nettle]))
AC_ARG_WITH([openssl],
  AS_HELP_STRING([--without-openssl], [Don't build support for mtree and xar hashes through openssl]))
case "$host_os" in
  *darwin* ) with_openssl=no ;;
esac

AC_ARG_WITH([xml2],
  AS_HELP_STRING([--without-xml2], [Don't build support for xar through libxml2]))
AC_ARG_WITH([expat],
  AS_HELP_STRING([--without-expat], [Don't build support for xar through expat]))

if test "x$with_xml2" != "xno"; then
  AC_PATH_PROG([XML2_CONFIG], [xml2-config],, [${PATH}])
  if test "x$XML2_CONFIG" != "x"; then
    CPPFLAGS="${CPPFLAGS} `${XML2_CONFIG} --cflags`"
    LIBS="${LIBS} `${XML2_CONFIG} --libs`"
    AC_CHECK_LIB(xml2,xmlInitParser,[true],AC_MSG_FAILURE(Missing xml2 library))
  else
    AC_CHECK_LIB(xml2,xmlInitParser)
  fi
  AC_CHECK_HEADERS([libxml/xmlreader.h libxml/xmlwriter.h])
fi
if test "x$ac_cv_header_libxml_xmlreader_h" != "xyes"; then
  if test "x$with_expat" != "xno"; then
    AC_CHECK_HEADERS([expat.h])
    AC_CHECK_LIB(expat,XML_ParserCreate)
  fi
fi

# TODO: Give the user the option of using a pre-existing system
# libarchive.  This will define HAVE_LIBARCHIVE which will cause
# bsdtar_platform.h to use #include <...> for the libarchive headers.
# Need to include Makefile.am magic to link against system
# -larchive in that case.
#AC_CHECK_LIB(archive,archive_version)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
# AC_TYPE_UID_T defaults to "int", which is incorrect for MinGW
# and MSVC. Use a customized version.
la_TYPE_UID_T
AC_TYPE_MODE_T
# AC_TYPE_OFF_T defaults to "long", which limits us to 4GB files on
# most systems... default to "long long" instead.
AC_CHECK_TYPE(off_t, [long long])
AC_TYPE_SIZE_T
AC_CHECK_TYPE(id_t, [unsigned long])
AC_CHECK_TYPE(uintptr_t, [unsigned int])

# Check for tm_gmtoff in struct tm
AC_CHECK_MEMBERS([struct tm.tm_gmtoff, struct tm.__tm_gmtoff],,,
[
#include <time.h>
])

# Check for f_namemax in struct statfs
AC_CHECK_MEMBERS([struct statfs.f_namemax],,,
[
#include <sys/param.h>
#include <sys/mount.h>
])

# Check for f_iosize in struct statvfs
AC_CHECK_MEMBERS([struct statvfs.f_iosize],,,
[
#include <sys/statvfs.h>
])

# Check for birthtime in struct stat
AC_CHECK_MEMBERS([struct stat.st_birthtime])

# Check for high-resolution timestamps in struct stat
AC_CHECK_MEMBERS([struct stat.st_birthtimespec.tv_nsec])
AC_CHECK_MEMBERS([struct stat.st_mtimespec.tv_nsec])
AC_CHECK_MEMBERS([struct stat.st_mtim.tv_nsec])
AC_CHECK_MEMBERS([struct stat.st_mtime_n]) # AIX
AC_CHECK_MEMBERS([struct stat.st_umtime]) # Tru64
AC_CHECK_MEMBERS([struct stat.st_mtime_usec]) # Hurd
# Check for block size support in struct stat
AC_CHECK_MEMBERS([struct stat.st_blksize])
# Check for st_flags in struct stat (BSD fflags)
AC_CHECK_MEMBERS([struct stat.st_flags])

# If you have uintmax_t, we assume printf supports %ju
# If you have unsigned long long, we assume printf supports %llu
# TODO: Check for %ju and %llu support directly.
AC_CHECK_TYPES([uintmax_t, unsigned long long])

# We use C99-style integer types
# Declare them if the local platform doesn't already do so.
AC_TYPE_INTMAX_T
AC_TYPE_UINTMAX_T
AC_TYPE_INT64_T
AC_TYPE_UINT64_T
AC_TYPE_INT32_T
AC_TYPE_UINT32_T
AC_TYPE_INT16_T
AC_TYPE_UINT16_T
AC_TYPE_UINT8_T

AC_CHECK_DECLS([SIZE_MAX, INT64_MAX, INT64_MIN, UINT64_MAX, UINT32_MAX])

AC_CHECK_DECL([SSIZE_MAX],
		[AC_DEFINE(HAVE_DECL_SSIZE_MAX, 1, [Define to 1 if you have the declaration of `SSIZE_MAX', and to 0 if you don't.])],
		[],
		[#include <limits.h>])

AC_CHECK_DECL([EFTYPE],
		[AC_DEFINE(HAVE_EFTYPE, 1, [A possible errno value for invalid file format errors])],
		[],
		[#include <errno.h>])
AC_CHECK_DECL([EILSEQ],
		[AC_DEFINE(HAVE_EILSEQ, 1, [A possible errno value for invalid file format errors])],
		[],
		[#include <errno.h>])
AC_CHECK_TYPE([wchar_t],
	        [AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_[]wchar_t), 1, [Define to 1 if the system has the type `wchar_t'.])dnl
		AC_CHECK_SIZEOF([wchar_t])],
		[])

AC_HEADER_TIME

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_HEADER_MAJOR
AC_FUNC_FSEEKO
AC_FUNC_MEMCMP
AC_FUNC_LSTAT
AC_FUNC_STAT
AC_FUNC_STRERROR_R
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
# check for:
#   CreateHardLinkA(LPCSTR, LPCSTR, LPSECURITY_ATTRIBUTES)
# To avoid necessity for including windows.h or special forward declaration
# workarounds, we use 'void *' for 'struct SECURITY_ATTRIBUTES *'
AC_CHECK_STDCALL_FUNC([CreateHardLinkA],[const char *, const char *, void *])
AC_CHECK_FUNCS([chflags chown chroot ctime_r])
AC_CHECK_FUNCS([fchdir fchflags fchmod fchown fcntl fdopendir fork])
AC_CHECK_FUNCS([fstat fstatat fstatfs fstatvfs ftruncate])
AC_CHECK_FUNCS([futimens futimes futimesat])
AC_CHECK_FUNCS([geteuid getpid getgrgid_r getgrnam_r])
AC_CHECK_FUNCS([getpwnam_r getpwuid_r getvfsbyname gmtime_r])
AC_CHECK_FUNCS([lchflags lchmod lchown link localtime_r lstat lutimes])
AC_CHECK_FUNCS([mbrtowc mbsnrtowcs memmove memset])
AC_CHECK_FUNCS([mkdir mkfifo mknod mkstemp])
AC_CHECK_FUNCS([nl_langinfo openat pipe poll readlink readlinkat])
AC_CHECK_FUNCS([select setenv setlocale sigaction statfs statvfs])
AC_CHECK_FUNCS([strchr strdup strerror strncpy_s strrchr symlink timegm])
AC_CHECK_FUNCS([tzset unsetenv utime utimensat utimes vfork])
AC_CHECK_FUNCS([wcrtomb wcscmp wcscpy wcslen wcsnrtombs wctomb wmemcmp wmemcpy])
AC_CHECK_FUNCS([_ctime64_s _fseeki64])
AC_CHECK_FUNCS([_get_timezone _localtime64_s _mkgmtime64])
# detects cygwin-1.7, as opposed to older versions
AC_CHECK_FUNCS([cygwin_conv_path])

# There are several variants of readdir_r around; we only
# accept the POSIX-compliant version.
AC_COMPILE_IFELSE(
 [AC_LANG_PROGRAM([[#include <dirent.h>]],
                  [[DIR *dir; struct dirent e, *r;
		    return(readdir_r(dir, &e, &r));]])],
 [AC_DEFINE(HAVE_READDIR_R,1,[Define to 1 if you have a POSIX compatible readdir_r])]
)

# FreeBSD's nl_langinfo supports an option to specify whether the
# current locale uses month/day or day/month ordering.  It makes the
# output a little prettier...
AC_CHECK_DECL([D_MD_ORDER],
[AC_DEFINE(HAVE_D_MD_ORDER, 1, [Define to 1 if nl_langinfo supports D_MD_ORDER])],
[],
[#if HAVE_LANGINFO_H
#include <langinfo.h>
#endif
])

# Check for dirent.d_namlen field explicitly
# (This is a bit more straightforward than, if not quite as portable as,
# the recipe given by the autoconf maintainers.)
AC_CHECK_MEMBER(struct dirent.d_namlen,,,
[#if HAVE_DIRENT_H
#include <dirent.h>
#endif
])

# Check for Extended Attributes support
AC_ARG_ENABLE([xattr],
		AS_HELP_STRING([--disable-xattr],
		[Enable Extended Attributes support (default: check)]))

if test "x$enable_xattr" != "xno"; then
	AC_CHECK_HEADERS([attr/xattr.h])
	AC_CHECK_HEADERS([sys/xattr.h sys/ea.h])
	AC_CHECK_LIB(attr,setxattr)
	AC_CHECK_FUNCS([extattr_get_file extattr_list_file])
	AC_CHECK_FUNCS([extattr_set_fd extattr_set_file])
	AC_CHECK_FUNCS([fgetxattr flistxattr fsetxattr getxattr])
	AC_CHECK_FUNCS([lgetxattr listxattr llistxattr lsetxattr])
	AC_CHECK_FUNCS([fgetea flistea fsetea getea])
	AC_CHECK_FUNCS([lgetea listea llistea lsetea])
	AC_CHECK_DECLS([EXTATTR_NAMESPACE_USER], [], [], [#include <sys/types.h>
#include <sys/extattr.h>
])
fi

# Check for ACL support
#
# The ACL support in libarchive is written against the POSIX1e draft,
# which was never officially approved and varies quite a bit across
# platforms.  Worse, some systems have completely non-POSIX acl functions,
# which makes the following checks rather more complex than I would like.
#
AC_ARG_ENABLE([acl],
		AS_HELP_STRING([--disable-acl],
		[Enable ACL support (default: check)]))

if test "x$enable_acl" != "xno"; then
   AC_CHECK_HEADERS([sys/acl.h])
   AC_CHECK_LIB([acl],[acl_get_file])
   AC_CHECK_FUNCS([acl_create_entry acl_init acl_set_fd acl_set_fd_np acl_set_file])

   AC_CHECK_TYPES(acl_permset_t,,,
	[#if HAVE_SYS_TYPES_H
	#include <sys/types.h>
	#endif
	#if HAVE_SYS_ACL_H
	#include <sys/acl.h>
	#endif
	])

    # The "acl_get_perm()" function was omitted from the POSIX draft.
    # (It's a pretty obvious oversight; otherwise, there's no way to
    # test for specific permissions in a permset.)  Linux uses the obvious
    # name, FreeBSD adds _np to mark it as "non-Posix extension."
    # Test for both as a double-check that we really have POSIX-style ACL support.
    AC_CHECK_FUNCS(acl_get_perm_np acl_get_perm acl_get_link acl_get_link_np,,,
	[#if HAVE_SYS_TYPES_H
	#include <sys/types.h>
	#endif
	#if HAVE_SYS_ACL_H
	#include <sys/acl.h>
	#endif
	])

    # MacOS has an acl.h that isn't POSIX.  It can be detected by
    # checking for ACL_USER
    AC_CHECK_DECL([ACL_USER],
		[AC_DEFINE(HAVE_ACL_USER, 1, [True for systems with POSIX ACL support])],
		[],
		[#include <sys/acl.h>])
fi

# Additional requirements
AC_SYS_LARGEFILE

dnl NOTE: Crypto checks must run last.
AC_DEFUN([CRYPTO_CHECK], [
  if test "$found_$1" != yes; then
    saved_LIBS="$LIBS"
    saved_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS -I. -I$srcdir -I$srcdir/libarchive"
    LIBS="$LIBS $mdLIBS $4"
    touch "check_crypto_md.h"
    AC_MSG_CHECKING([support for ARCHIVE_CRYPTO_$1_$2])
    AC_LINK_IFELSE([AC_LANG_SOURCE([
#define ARCHIVE_$1_COMPILE_TEST
#define ARCHIVE_CRYPTO_$1_$2
#define PLATFORM_CONFIG_H "check_crypto_md.h"

$(cat "$srcdir/libarchive/archive_crypto.c")

int
main(int argc, char **argv)
{
  archive_$3_ctx ctx;
  archive_$3_init(&ctx);
  archive_$3_update(&ctx, *argv, argc);
  archive_$3_final(&ctx, NULL);
  return 0;
}
])],
    [ AC_MSG_RESULT([yes])
      found_$1=yes
      if [[ -n "$4" ]]; then
        # The .$4 avoids -e, which doesn't work on Solaris 10
	# The _$mdLIBS allows .$4 to match at beginning of string <sigh>
        test -n "$(echo _$mdLIBS | $GREP .$4)" || mdLIBS="$mdLIBS $4"
      fi
      AC_DEFINE(ARCHIVE_CRYPTO_$1_$2, 1, [ $1 via ARCHIVE_CRYPTO_$1_$2 supported.])
    ],
    [ AC_MSG_RESULT([no])])
    LIBS="$saved_LIBS"
    CPPFLAGS="$saved_CPPFLAGS"
    rm "check_crypto_md.h"
  fi
])

CRYPTO_CHECK(MD5, LIBC, md5)
CRYPTO_CHECK(MD5, LIBSYSTEM, md5)
CRYPTO_CHECK(RMD160, LIBC, rmd160)
CRYPTO_CHECK(SHA1, LIBC, sha1)
CRYPTO_CHECK(SHA1, LIBSYSTEM, sha1)
CRYPTO_CHECK(SHA256, LIBC, sha256)
CRYPTO_CHECK(SHA256, LIBC2, sha256)
CRYPTO_CHECK(SHA256, LIBC3, sha256)
CRYPTO_CHECK(SHA256, LIBSYSTEM, sha256)
CRYPTO_CHECK(SHA384, LIBC, sha384)
CRYPTO_CHECK(SHA384, LIBC2, sha384)
CRYPTO_CHECK(SHA384, LIBC3, sha384)
CRYPTO_CHECK(SHA384, LIBSYSTEM, sha384)
CRYPTO_CHECK(SHA512, LIBC, sha512)
CRYPTO_CHECK(SHA512, LIBC2, sha512)
CRYPTO_CHECK(SHA512, LIBC3, sha512)
CRYPTO_CHECK(SHA512, LIBSYSTEM, sha512)

if test "x$with_nettle" != "xno"; then
    CRYPTO_CHECK(MD5, NETTLE, md5, -lnettle)
    CRYPTO_CHECK(RMD160, NETTLE, rmd160, -lnettle)
    CRYPTO_CHECK(SHA1, NETTLE, sha1, -lnettle)
    CRYPTO_CHECK(SHA256, NETTLE, sha256, -lnettle)
    CRYPTO_CHECK(SHA384, NETTLE, sha384, -lnettle)
    CRYPTO_CHECK(SHA512, NETTLE, sha512, -lnettle)
fi
if test "x$with_openssl" != "xno"; then
    CRYPTO_CHECK(MD5, OPENSSL, md5, -lcrypto)
    CRYPTO_CHECK(RMD160, OPENSSL, rmd160, -lcrypto)
    CRYPTO_CHECK(SHA1, OPENSSL, sha1, -lcrypto)
    CRYPTO_CHECK(SHA256, OPENSSL, sha256, -lcrypto)
    CRYPTO_CHECK(SHA384, OPENSSL, sha384, -lcrypto)
    CRYPTO_CHECK(SHA512, OPENSSL, sha512, -lcrypto)
fi

# Probe libmd AFTER OpenSSL/libcrypto.
# The two are incompatible and OpenSSL is more complete.
CRYPTO_CHECK(MD5, LIBMD, md5, -lmd)
CRYPTO_CHECK(RMD160, LIBMD, rmd160, -lmd)
CRYPTO_CHECK(SHA1, LIBMD, sha1, -lmd)
CRYPTO_CHECK(SHA256, LIBMD, sha256, -lmd)
CRYPTO_CHECK(SHA512, LIBMD, sha512, -lmd)

LIBS="$LIBS $mdLIBS"

AC_OUTPUT
