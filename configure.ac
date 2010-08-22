dnl Process this file with autoconf to produce a configure script.

dnl First, define all of the version numbers up front.
dnl In particular, this allows the version macro to be used in AC_INIT

dnl These first two version numbers are updated automatically on each release.
m4_define([LIBTRANSFORM_VERSION_S],[3.0.0a])
m4_define([LIBTRANSFORM_VERSION_N],[3000000])

AC_PREREQ(2.65)

#
# Now starts the "real" configure script.
#

AC_INIT([libtransform],LIBTRANSFORM_VERSION_S(),[kientzle@freebsd.org])
# Make sure the srcdir contains "libtransform" directory
AC_CONFIG_SRCDIR([libtransform])
# Use auxiliary subscripts from this subdirectory (cleans up root)
AC_CONFIG_AUX_DIR([build/autoconf])
# M4 scripts
AC_CONFIG_MACRO_DIR([build/autoconf])
# Must follow AC_CONFIG macros above...
AM_INIT_AUTOMAKE()

# Libtool versioning uses different conventions on different
# platforms.  At least on FreeBSD, libtool uses an overly complex
# convention that attempts to solve problems that most people just
# don't have and which just causes confusion for most end users.
TRANSFORM_MAJOR=$(( LIBTRANSFORM_VERSION_N() / 1000000 ))
TRANSFORM_MINOR=$(( (LIBTRANSFORM_VERSION_N() / 1000) % 1000 ))
TRANSFORM_REVISION=$(( LIBTRANSFORM_VERSION_N() % 1000 ))
TRANSFORM_LIBTOOL_MAJOR=`echo $((${TRANSFORM_MAJOR} + ${TRANSFORM_MINOR}))`
TRANSFORM_LIBTOOL_VERSION=$TRANSFORM_LIBTOOL_MAJOR:$TRANSFORM_REVISION:$TRANSFORM_MINOR

# Stick the version numbers into config.h
AC_DEFINE([LIBTRANSFORM_VERSION_STRING],"LIBTRANSFORM_VERSION_S()",
	[Version number of libtransform])
AC_DEFINE_UNQUOTED([LIBTRANSFORM_VERSION_NUMBER],"LIBTRANSFORM_VERSION_N()",
	[Version number of libtransform as a single integer])

# The shell variables here must be the same as the AC_SUBST() variables
# below, but the shell variable names apparently cannot be the same as
# the m4 macro names above.  Why?  Ask autoconf.
LIBTRANSFORM_VERSION_STRING=LIBTRANSFORM_VERSION_S()
LIBTRANSFORM_VERSION_NUMBER=LIBTRANSFORM_VERSION_N()

# Substitute the above version numbers into the various files below.
# Yes, I believe this is the fourth time we define what are essentially
# the same symbols.  Why? Ask autoconf.
AC_SUBST(TRANSFORM_LIBTOOL_VERSION)
AC_SUBST(LIBTRANSFORM_VERSION_STRING)
AC_SUBST(LIBTRANSFORM_VERSION_NUMBER)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([build/pkgconfig/libtransform.pc])

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

# Set up defines needed before including any headers
case $host in
  *mingw* | *cygwin* )
  AC_DEFINE([_WIN32_WINNT], 0x0500, [Define to '0x0500' for Windows 2000 APIs.])
  AC_DEFINE([WINVER], 0x0500, [Define to '0x0500' for Windows 2000 APIs.])
  ;;
esac

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([ctype.h errno.h])
AC_CHECK_HEADERS([ext2fs/ext2_fs.h fcntl.h grp.h])
AC_CHECK_HEADERS([inttypes.h io.h langinfo.h limits.h])
AC_CHECK_HEADERS([linux/fiemap.h linux/fs.h])
AC_CHECK_HEADERS([locale.h paths.h poll.h pwd.h regex.h signal.h stdarg.h])
AC_CHECK_HEADERS([stdint.h stdlib.h string.h])
AC_CHECK_HEADERS([sys/cdefs.h sys/extattr.h sys/ioctl.h sys/mkdev.h])
AC_CHECK_HEADERS([sys/param.h sys/poll.h sys/select.h])
AC_CHECK_HEADERS([sys/utsname.h time.h unistd.h utime.h wchar.h wctype.h])
AC_CHECK_HEADERS([sys/time.h sys/utime.h sys/utsname.h])
AC_CHECK_HEADERS([time.h unistd.h utime.h wchar.h wctype.h])
AC_CHECK_HEADERS([windows.h winioctl])

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

AC_ARG_WITH([lzma],
  AS_HELP_STRING([--without-lzma], [Don't build support for xz through lzma]))

if test "x$with_lzma" != "xno"; then
  AC_CHECK_HEADERS([lzma.h])
  AC_CHECK_LIB(lzma,lzma_stream_decoder)
fi

# TODO: Give the user the option of using a pre-existing system
# libtransform.  This will define HAVE_LIBTRANSFORM which will cause
# bsdtar_platform.h to use #include <...> for the libtransform headers.
# Need to include Makefile.am magic to link against system
# -ltransform in that case.
#AC_CHECK_LIB(transform,transform_version)

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

# Check for block size support in struct stat
AC_CHECK_MEMBERS([struct stat.st_blksize])
# Check for st_flags in struct stat (BSD fflags)
AC_CHECK_MEMBERS([struct stat.st_flags])

# If you have uintmax_t, we assume printf supports %ju
# If you have unsigned long long, we assume printf supports %llu
# TODO: Check for %ju and %llu support directly.
AC_CHECK_TYPES([uintmax_t, unsigned long long])

# We need int64_t, uint64_t, intmax_t, and uintmax_t
AC_TYPE_INTMAX_T
AC_TYPE_INT64_T
AC_TYPE_UINTMAX_T
AC_TYPE_UINT64_T

# TODO: If any of these are missing, define them right here.
AC_CHECK_DECLS([SIZE_MAX, SSIZE_MAX, INT64_MAX, INT64_MIN, UINT64_MAX, UINT32_MAX])

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
AC_CHECK_FUNCS([fchdir fchflags fchmod fchown fcntl fork])
AC_CHECK_FUNCS([fstat ftruncate futimens futimes])
AC_CHECK_FUNCS([lchflags lchmod lchown link localtime_r lstat])
AC_CHECK_FUNCS([brtowc memmove memset mkdir mkfifo mknod mkstemp])
AC_CHECK_FUNCS([nl_langinfo pipe poll readlink])
AC_CHECK_FUNCS([select setenv setlocale sigaction])
AC_CHECK_FUNCS([strchr strdup strerror strncpy_s strrchr symlink timegm])
AC_CHECK_FUNCS([tzset unsetenv utime utimensat utimes vfork])
AC_CHECK_FUNCS([wcrtomb wcscmp wcscpy wcslen wctomb wmemcmp wmemcpy])
# detects cygwin-1.7, as opposed to older versions
AC_CHECK_FUNCS([cygwin_conv_path])

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

# Additional requirements
AC_SYS_LARGEFILE

AC_OUTPUT
