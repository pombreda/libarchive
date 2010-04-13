#
#
PROJECT(libtransform C)
#
CMAKE_MINIMUM_REQUIRED(VERSION 2.6.3 FATAL_ERROR)
SET(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build/cmake")
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${libtransform_BINARY_DIR}/bin)
endif()

#
# Version - read from 'version' file.
#
FILE(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/build/version _version)
STRING(REGEX REPLACE
 "^([0-9])[0-9][0-9][0-9][0-9][0-9][0-9][a-z]?$" "\\1" _major ${_version})
STRING(REGEX REPLACE
 "^[0-9]([0-9][0-9][0-9])[0-9][0-9][0-9][a-z]?$" "\\1" _minor ${_version})
STRING(REGEX REPLACE
 "^[0-9][0-9][0-9][0-9]([0-9][0-9][0-9])[a-z]?$" "\\1" _revision ${_version})
STRING(REGEX REPLACE
 "^[0-9][0-9][0-9][0-9][0-9][0-9][0-9]([a-z]?)$" "\\1" _quality ${_version})
SET(_version_number ${_major}${_minor}${_revision})
STRING(REGEX REPLACE "[0]*([^0][0-9]*)$" "\\1" _minor ${_minor})
STRING(REGEX REPLACE "[0]*([^0][0-9]*)$" "\\1" _revision ${_revision})
#
SET(VERSION                    "${_major}.${_minor}.${_revision}${_quality}")
SET(BSDCPIO_VERSION_STRING     "${VERSION}")
SET(BSDTAR_VERSION_STRING      "${VERSION}")
SET(LIBTRANSFORM_VERSION_NUMBER  "${_version_number}")
SET(LIBTRANSFORM_VERSION_STRING  "${VERSION}")
# Shared library number
SET(SOVERSION                   8)

# Enable CTest/CDash support
include(CTest)

# Provide ADD_TEST_28 macro to approximate CMake 2.8 ADD_TEST(NAME).
# TODO: Require CMake 2.8 and drop this workaround (perhaps late 2010).
INCLUDE(AddTest28)

OPTION(ENABLE_TAR_SHARED "Enable dynamic build of tar" OFF)
OPTION(ENABLE_TEST "Enable unit and regression tests" ON)

IF(ENABLE_TEST)
	ENABLE_TESTING()
ENDIF(ENABLE_TEST)
IF(WIN32 AND NOT CYGWIN)
  # Currently, dynamic build only.
  SET(ENABLE_TAR_SHARED ON)
  SET(ENABLE_CPIO_SHARED ON)
ENDIF(WIN32 AND NOT CYGWIN)

IF(WIN32)
  SET(_WIN32_WINNT 0x0500 CACHE INTERNAL "Setting _WIN32_WINNT to 0x0500 for Windows 2000 APIs")
  SET(WINVER 0x0500 CACHE INTERNAL "Setting WINVER to 0x0500 for Windows 2000 APIs")
ENDIF(WIN32)

#
INCLUDE(CheckCSourceRuns)
INCLUDE(CheckFileOffsetBits)
INCLUDE(CheckFuncs)
INCLUDE(CheckHeaderDirent)
INCLUDE(CheckIncludeFile)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckStructMember)
INCLUDE(CheckSymbolExists)
INCLUDE(CheckTypeExists)
INCLUDE(CheckTypeSize)

#
# Generate list.h
#
MACRO (GENERATE_LIST_H _listfile _cmlist __list_sources)
  SET(_argv ${ARGV})
  # Remove _listfile and _cmlist from _argv
  LIST(REMOVE_AT _argv 0 1)
  IF (NOT EXISTS "${_listfile}" OR
     ${_cmlist} IS_NEWER_THAN "${_listfile}")

    MESSAGE(STATUS "Generating ${_listfile}")
    FILE(WRITE ${_listfile} "")
    FOREACH (testfile ${_argv})
      IF (testfile MATCHES "^test_[^/]+[.]c$")
        FILE(STRINGS ${testfile} testvar REGEX "^DEFINE_TEST")
        FOREACH (deftest ${testvar})
          FILE(APPEND ${_listfile} "${deftest}\n")
        ENDFOREACH (deftest)
      ENDIF (testfile MATCHES "^test_[^/]+[.]c$")
    ENDFOREACH (testfile)

  ENDIF (NOT EXISTS "${_listfile}" OR
     ${_cmlist} IS_NEWER_THAN "${_listfile}")
ENDMACRO (GENERATE_LIST_H)
#
# Generate installation rules for man pages.
#
MACRO (INSTALL_MAN __mans)
  FOREACH (_man ${ARGV})
    STRING(REGEX REPLACE "^.+[.]([1-9])" "\\1" _mansect ${_man})
    INSTALL(FILES ${_man} DESTINATION "share/man/man${_mansect}")
  ENDFOREACH (_man)
ENDMACRO (INSTALL_MAN __mans)

#
# Check compress/decompress libraries
#
IF(WIN32 AND NOT CMAKE_CL_64 AND NOT CYGWIN)
  # GnuWin32 is only for Win32, not Win64.
  SET(__GNUWIN32PATH "C:/Program Files/GnuWin32")
ENDIF(WIN32 AND NOT CMAKE_CL_64 AND NOT CYGWIN)
IF(DEFINED __GNUWIN32PATH AND EXISTS "${__GNUWIN32PATH}")
  # You have to add a path availabel DLL file into PATH environment variable.
  # Maybe DLL path is "C:/Program Files/GnuWin32/bin".
  # The zlib and the bzip2 Setup program have installed programs and DLLs into
  # "C:/Program Files/GnuWin32" by default.
  # This is convenience setting for Windows.
  SET(CMAKE_PREFIX_PATH ${__GNUWIN32PATH} $(CMAKE_PREFIX_PATH))
  #
  # If you didn't use Setup program or installed into nonstandard path,
  # cmake cannot find out your zlib or bzip2 libraries and include files,
  # you should execute cmake with  -DCMAKE_PREFIX_PATH option.
  #   e.g.
  #     cmake -DCMAKE_PREFIX_PATH=<your-GnuWin32-path> <path-to-source>
  #
  # If compiling error occured in zconf.h, You may need patch to zconf.h.
  #--- zconf.h.orig	2005-07-21 00:40:26.000000000
  #+++ zconf.h	2009-01-19 11:39:10.093750000
  #@@ -286,7 +286,7 @@
  # 
  # #if 1           /* HAVE_UNISTD_H -- this line is updated by ./configure */
  # #  include <sys/types.h> /* for off_t */
  #-#  include <unistd.h>    /* for SEEK_* and off_t */
  #+#  include <stdio.h>    /* for SEEK_* and off_t */
  # #  ifdef VMS
  # #    include <unixio.h>   /* for off_t */
  # #  endif
ENDIF(DEFINED __GNUWIN32PATH AND EXISTS "${__GNUWIN32PATH}")

SET(ADDITIONAL_LIBS "")
#
# Find ZLIB
#
FIND_PACKAGE(ZLIB)
IF(ZLIB_FOUND)
  SET(HAVE_LIBZ 1)
  SET(HAVE_ZLIB_H 1)
  INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR})
  LIST(APPEND ADDITIONAL_LIBS ${ZLIB_LIBRARIES})
ENDIF(ZLIB_FOUND)
MARK_AS_ADVANCED(CLEAR ZLIB_INCLUDE_DIR)
MARK_AS_ADVANCED(CLEAR ZLIB_LIBRARY)
#
# Find BZip2
#
FIND_PACKAGE(BZip2)
IF(BZIP2_FOUND)
  SET(HAVE_LIBBZ2 1)
  SET(HAVE_BZLIB_H 1)
  INCLUDE_DIRECTORIES(${BZIP2_INCLUDE_DIR})
  LIST(APPEND ADDITIONAL_LIBS ${BZIP2_LIBRARIES})
ENDIF(BZIP2_FOUND)
MARK_AS_ADVANCED(CLEAR BZIP2_INCLUDE_DIR)
MARK_AS_ADVANCED(CLEAR BZIP2_LIBRARY)
#
# Find LZMA
#
FIND_PACKAGE(LZMA)
IF(LZMA_FOUND)
  SET(HAVE_LIBLZMA 1)
  SET(HAVE_LZMA_H 1)
  INCLUDE_DIRECTORIES(${LZMA_INCLUDE_DIR})
  LIST(APPEND ADDITIONAL_LIBS ${LZMA_LIBRARIES})
  MARK_AS_ADVANCED(CLEAR LZMA_INCLUDE_DIR)
  MARK_AS_ADVANCED(CLEAR LZMA_LIBRARY)
ELSEIF(LZMADEC_FOUND)
  SET(HAVE_LIBLZMADEC 1)
  SET(HAVE_LZMADEC_H 1)
  INCLUDE_DIRECTORIES(${LZMADEC_INCLUDE_DIR})
  LIST(APPEND ADDITIONAL_LIBS ${LZMADEC_LIBRARIES})
  MARK_AS_ADVANCED(CLEAR LZMADEC_INCLUDE_DIR)
  MARK_AS_ADVANCED(CLEAR LZMADEC_LIBRARY)
ELSE(LZMA_FOUND)
  MARK_AS_ADVANCED(CLEAR LZMA_INCLUDE_DIR)
  MARK_AS_ADVANCED(CLEAR LZMA_LIBRARY)
ENDIF(LZMA_FOUND)

#
# Check headers
#
CHECK_HEADER_DIRENT()

SET(INCLUDES "")
MACRO (LA_CHECK_INCLUDE_FILE header var)
      CHECK_INCLUDE_FILES("${INCLUDES};${header}" ${var})
      IF (${var})
      	 SET(INCLUDES ${INCLUDES} ${header})
      ENDIF (${var})
ENDMACRO (LA_CHECK_INCLUDE_FILE)

# Few headers that must precede other headers
# Must precede sys/extattr.h on FreeBSD
LA_CHECK_INCLUDE_FILE("sys/types.h" HAVE_SYS_TYPES_H)

# Alphabetize the rest unless there's a compelling reason
LA_CHECK_INCLUDE_FILE("ctype.h" HAVE_CTYPE_H)
LA_CHECK_INCLUDE_FILE("direct.h" HAVE_DIRECT_H)
LA_CHECK_INCLUDE_FILE("dlfcn.h" HAVE_DLFCN_H)
LA_CHECK_INCLUDE_FILE("errno.h" HAVE_ERRNO_H)
LA_CHECK_INCLUDE_FILE("ext2fs/ext2_fs.h" HAVE_EXT2FS_EXT2_FS_H)
LA_CHECK_INCLUDE_FILE("fcntl.h" HAVE_FCNTL_H)
LA_CHECK_INCLUDE_FILE("grp.h" HAVE_GRP_H)
LA_CHECK_INCLUDE_FILE("inttypes.h" HAVE_INTTYPES_H)
LA_CHECK_INCLUDE_FILE("io.h" HAVE_IO_H)
LA_CHECK_INCLUDE_FILE("langinfo.h" HAVE_LANGINFO_H)
LA_CHECK_INCLUDE_FILE("limits.h" HAVE_LIMITS_H)
LA_CHECK_INCLUDE_FILE("linux/fiemap.h" HAVE_LINUX_FIEMAP_H)
LA_CHECK_INCLUDE_FILE("linux/fs.h" HAVE_LINUX_FS_H)
LA_CHECK_INCLUDE_FILE("locale.h" HAVE_LOCALE_H)
LA_CHECK_INCLUDE_FILE("memory.h" HAVE_MEMORY_H)
LA_CHECK_INCLUDE_FILE("paths.h" HAVE_PATHS_H)
LA_CHECK_INCLUDE_FILE("poll.h" HAVE_POLL_H)
LA_CHECK_INCLUDE_FILE("process.h" HAVE_PROCESS_H)
LA_CHECK_INCLUDE_FILE("pwd.h" HAVE_PWD_H)
LA_CHECK_INCLUDE_FILE("regex.h" HAVE_REGEX_H)
LA_CHECK_INCLUDE_FILE("signal.h" HAVE_SIGNAL_H)
LA_CHECK_INCLUDE_FILE("stdarg.h" HAVE_STDARG_H)
LA_CHECK_INCLUDE_FILE("stdint.h" HAVE_STDINT_H)
LA_CHECK_INCLUDE_FILE("stdlib.h" HAVE_STDLIB_H)
LA_CHECK_INCLUDE_FILE("string.h" HAVE_STRING_H)
LA_CHECK_INCLUDE_FILE("strings.h" HAVE_STRINGS_H)
LA_CHECK_INCLUDE_FILE("sys/cdefs.h" HAVE_SYS_CDEFS_H)
LA_CHECK_INCLUDE_FILE("sys/extattr.h" HAVE_SYS_EXTATTR_H)
LA_CHECK_INCLUDE_FILE("sys/ioctl.h" HAVE_SYS_IOCTL_H)
LA_CHECK_INCLUDE_FILE("sys/mkdev.h" HAVE_SYS_MKDEV_H)
LA_CHECK_INCLUDE_FILE("sys/param.h" HAVE_SYS_PARAM_H)
LA_CHECK_INCLUDE_FILE("sys/poll.h" HAVE_SYS_POLL_H)
LA_CHECK_INCLUDE_FILE("sys/select.h" HAVE_SYS_SELECT_H)
LA_CHECK_INCLUDE_FILE("sys/stat.h" HAVE_SYS_STAT_H)
LA_CHECK_INCLUDE_FILE("sys/time.h" HAVE_SYS_TIME_H)
LA_CHECK_INCLUDE_FILE("sys/utime.h" HAVE_SYS_UTIME_H)
LA_CHECK_INCLUDE_FILE("sys/utsname.h" HAVE_SYS_UTSNAME_H)
LA_CHECK_INCLUDE_FILE("sys/wait.h" HAVE_SYS_WAIT_H)
LA_CHECK_INCLUDE_FILE("time.h" HAVE_TIME_H)
LA_CHECK_INCLUDE_FILE("unistd.h" HAVE_UNISTD_H)
LA_CHECK_INCLUDE_FILE("utime.h" HAVE_UTIME_H)
LA_CHECK_INCLUDE_FILE("wchar.h" HAVE_WCHAR_H)
LA_CHECK_INCLUDE_FILE("wctype.h" HAVE_WCTYPE_H)
LA_CHECK_INCLUDE_FILE("windows.h" HAVE_WINDOWS_H)
LA_CHECK_INCLUDE_FILE("winioctl.h" HAVE_WINIOCTL_H)


#
# Some headers require extra includes when they're available.
#

SET(CMAKE_REQUIRED_LIBRARIES "")

#
# Check functions
#
CHECK_SYMBOL_EXISTS(CreateHardLinkA "windows.h" HAVE_CREATEHARDLINKA)
CHECK_SYMBOL_EXISTS(CreateHardLinkW "windows.h" HAVE_CREATEHARDLINKW)
CHECK_SYMBOL_EXISTS(_CrtSetReportMode "crtdbg.h" HAVE__CrtSetReportMode)
CHECK_FUNCTION_EXISTS_GLIBC(chflags HAVE_CHFLAGS)
CHECK_FUNCTION_EXISTS_GLIBC(chown HAVE_CHOWN)
CHECK_FUNCTION_EXISTS_GLIBC(chroot HAVE_CHROOT)
CHECK_FUNCTION_EXISTS_GLIBC(ctime_r HAVE_CTIME_R)
CHECK_FUNCTION_EXISTS_GLIBC(fchdir HAVE_FCHDIR)
CHECK_FUNCTION_EXISTS_GLIBC(fchflags HAVE_FCHFLAGS)
CHECK_FUNCTION_EXISTS_GLIBC(fchmod HAVE_FCHMOD)
CHECK_FUNCTION_EXISTS_GLIBC(fchown HAVE_FCHOWN)
CHECK_FUNCTION_EXISTS_GLIBC(fcntl HAVE_FCNTL)
CHECK_FUNCTION_EXISTS_GLIBC(fork HAVE_FORK)
CHECK_FUNCTION_EXISTS_GLIBC(fstat HAVE_FSTAT)
CHECK_FUNCTION_EXISTS_GLIBC(ftruncate HAVE_FTRUNCATE)
CHECK_FUNCTION_EXISTS_GLIBC(futimens HAVE_FUTIMENS)
CHECK_FUNCTION_EXISTS_GLIBC(futimes HAVE_FUTIMES)
CHECK_FUNCTION_EXISTS_GLIBC(geteuid HAVE_GETEUID)
CHECK_FUNCTION_EXISTS_GLIBC(getgrnam_r HAVE_GETGRNAM_R)
CHECK_FUNCTION_EXISTS_GLIBC(getpwnam_r HAVE_GETPWNAM_R)
CHECK_FUNCTION_EXISTS_GLIBC(getpid HAVE_GETPID)
CHECK_FUNCTION_EXISTS_GLIBC(lchflags HAVE_LCHFLAGS)
CHECK_FUNCTION_EXISTS_GLIBC(lchmod HAVE_LCHMOD)
CHECK_FUNCTION_EXISTS_GLIBC(lchown HAVE_LCHOWN)
CHECK_FUNCTION_EXISTS_GLIBC(link HAVE_LINK)
CHECK_FUNCTION_EXISTS_GLIBC(localtime_r HAVE_LOCALTIME_R)
CHECK_FUNCTION_EXISTS_GLIBC(lstat HAVE_LSTAT)
CHECK_FUNCTION_EXISTS_GLIBC(lutimes HAVE_LUTIMES)
CHECK_FUNCTION_EXISTS_GLIBC(mbrtowc HAVE_MBRTOWC)
CHECK_FUNCTION_EXISTS_GLIBC(memmove HAVE_MEMMOVE)
CHECK_FUNCTION_EXISTS_GLIBC(mkdir HAVE_MKDIR)
CHECK_FUNCTION_EXISTS_GLIBC(mkfifo HAVE_MKFIFO)
CHECK_FUNCTION_EXISTS_GLIBC(mknod HAVE_MKNOD)
CHECK_FUNCTION_EXISTS_GLIBC(mkstemp HAVE_MKSTEMP)
CHECK_FUNCTION_EXISTS_GLIBC(nl_langinfo HAVE_NL_LANGINFO)
CHECK_FUNCTION_EXISTS_GLIBC(pipe HAVE_PIPE)
CHECK_FUNCTION_EXISTS_GLIBC(poll HAVE_POLL)
CHECK_FUNCTION_EXISTS_GLIBC(readlink HAVE_READLINK)
CHECK_FUNCTION_EXISTS_GLIBC(select HAVE_SELECT)
CHECK_FUNCTION_EXISTS_GLIBC(setenv HAVE_SETENV)
CHECK_FUNCTION_EXISTS_GLIBC(setlocale HAVE_SETLOCALE)
CHECK_FUNCTION_EXISTS_GLIBC(sigaction HAVE_SIGACTION)
CHECK_FUNCTION_EXISTS_GLIBC(strchr HAVE_STRCHR)
CHECK_FUNCTION_EXISTS_GLIBC(strdup HAVE_STRDUP)
CHECK_FUNCTION_EXISTS_GLIBC(strerror HAVE_STRERROR)
CHECK_FUNCTION_EXISTS_GLIBC(strncpy_s HAVE_STRNCPY_S)
CHECK_FUNCTION_EXISTS_GLIBC(strrchr HAVE_STRRCHR)
CHECK_FUNCTION_EXISTS_GLIBC(symlink HAVE_SYMLINK)
CHECK_FUNCTION_EXISTS_GLIBC(timegm HAVE_TIMEGM)
CHECK_FUNCTION_EXISTS_GLIBC(tzset HAVE_TZSET)
CHECK_FUNCTION_EXISTS_GLIBC(unsetenv HAVE_UNSETENV)
CHECK_FUNCTION_EXISTS_GLIBC(utime HAVE_UTIME)
CHECK_FUNCTION_EXISTS_GLIBC(utimes HAVE_UTIMES)
CHECK_FUNCTION_EXISTS_GLIBC(utimensat HAVE_UTIMENSAT)
CHECK_FUNCTION_EXISTS_GLIBC(vfork HAVE_VFORK)
CHECK_FUNCTION_EXISTS_GLIBC(wcrtomb HAVE_WCRTOMB)
CHECK_FUNCTION_EXISTS_GLIBC(wcscpy HAVE_WCSCPY)
CHECK_FUNCTION_EXISTS_GLIBC(wcslen HAVE_WCSLEN)
CHECK_FUNCTION_EXISTS_GLIBC(wctomb HAVE_WCTOMB)
CHECK_FUNCTION_EXISTS_GLIBC(_ctime64_s HAVE__CTIME64_S)
CHECK_FUNCTION_EXISTS_GLIBC(_get_timezone HAVE__GET_TIMEZONE)
CHECK_FUNCTION_EXISTS_GLIBC(_localtime64_s HAVE__LOCALTIME64_S)
CHECK_FUNCTION_EXISTS_GLIBC(_mkgmtime64 HAVE__MKGMTIME64)
CHECK_SYMBOL_EXISTS(wmemcmp "wchar.h" HAVE_WMEMCMP)
CHECK_SYMBOL_EXISTS(wmemcpy "wchar.h" HAVE_WMEMCPY)

SET(CMAKE_REQUIRED_LIBRARIES "")
CHECK_SYMBOL_EXISTS(fseeko           "stdio.h"    HAVE_FSEEKO)
CHECK_SYMBOL_EXISTS(strerror_r       "string.h"   HAVE_STRERROR_R)
CHECK_SYMBOL_EXISTS(strftime         "time.h"     HAVE_STRFTIME)
CHECK_SYMBOL_EXISTS(vprintf          "stdio.h"    HAVE_VPRINTF)
CHECK_SYMBOL_EXISTS(cygwin_conv_path "sys/cygwin.h" HAVE_CYGWIN_CONV_PATH)

CHECK_SYMBOL_EXISTS(major            "sys/mkdev.h"     MAJOR_IN_MKDEV)
CHECK_SYMBOL_EXISTS(major            "sys/sysmacros.h" MAJOR_IN_SYSMACROS)

IF(HAVE_STRERROR_R)
  SET(HAVE_DECL_STRERROR_R 1)
ENDIF(HAVE_STRERROR_R)

#
# Check defines
#
SET(headers "limits.h")
IF(HAVE_STDINT_H)
  LIST(APPEND headers "stdint.h")
ENDIF(HAVE_STDINT_H)
IF(HAVE_INTTYPES_H)
  LIST(APPEND headers "inttypes.h")
ENDIF(HAVE_INTTYPES_H)
CHECK_SYMBOL_EXISTS(EFTYPE           "errno.h"    HAVE_EFTYPE)
CHECK_SYMBOL_EXISTS(EILSEQ           "errno.h"    HAVE_EILSEQ)
CHECK_SYMBOL_EXISTS(D_MD_ORDER       "langinfo.h" HAVE_D_MD_ORDER)
CHECK_SYMBOL_EXISTS(optarg           "unistd.h"   HAVE_DECL_OPTARG)
CHECK_SYMBOL_EXISTS(optind           "unistd.h"   HAVE_DECL_OPTIND)
CHECK_SYMBOL_EXISTS(INT64_MAX        "${headers}" HAVE_DECL_INT64_MAX)
CHECK_SYMBOL_EXISTS(INT64_MIN        "${headers}" HAVE_DECL_INT64_MIN)
CHECK_SYMBOL_EXISTS(UINT32_MAX       "${headers}" HAVE_DECL_UINT32_MAX)
CHECK_SYMBOL_EXISTS(UINT64_MAX       "${headers}" HAVE_DECL_UINT64_MAX)
CHECK_SYMBOL_EXISTS(SIZE_MAX         "${headers}" HAVE_DECL_SIZE_MAX)
CHECK_SYMBOL_EXISTS(SSIZE_MAX        "limits.h"   HAVE_DECL_SSIZE_MAX)

#
# Check struct members
#
# Check for tm_gmtoff in struct tm
CHECK_STRUCT_MEMBER("struct tm" tm_gmtoff
    "time.h" HAVE_STRUCT_TM_TM_GMTOFF)
CHECK_STRUCT_MEMBER("struct tm" __tm_gmtoff
    "time.h" HAVE_STRUCT_TM___TM_GMTOFF)

# Check for birthtime in struct stat
CHECK_STRUCT_MEMBER("struct stat" st_birthtime
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_BIRTHTIME)

# Check for high-resolution timestamps in struct stat
CHECK_STRUCT_MEMBER("struct stat" st_birthtimespec.tv_nsec
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_BIRTHTIMESPEC_TV_NSEC)
CHECK_STRUCT_MEMBER("struct stat" st_mtimespec.tv_nsec
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC)
CHECK_STRUCT_MEMBER("struct stat" st_mtim.tv_nsec
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC)
CHECK_STRUCT_MEMBER("struct stat" st_mtime_n
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_MTIME_N)
CHECK_STRUCT_MEMBER("struct stat" st_umtime
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_UMTIME)
CHECK_STRUCT_MEMBER("struct stat" st_mtime_usec
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_MTIME_USEC)
# Check for block size support in struct stat
CHECK_STRUCT_MEMBER("struct stat" st_blksize
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_BLKSIZE)
# Check for st_flags in struct stat (BSD fflags)
CHECK_STRUCT_MEMBER("struct stat" st_flags
    "sys/types.h;sys/stat.h" HAVE_STRUCT_STAT_ST_FLAGS)
#
#
CHECK_STRUCT_MEMBER("struct tm" tm_sec
    "sys/types.h;sys/time.h;time.h" TIME_WITH_SYS_TIME)

#
# Check for integer types
#
# XXX There must be a way to make this simpler <sigh> XXXX
#
CHECK_TYPE_SIZE("long long int"            LONG_LONG_INT)
CHECK_TYPE_SIZE("unsigned long long"       UNSIGNED_LONG_LONG)
CHECK_TYPE_SIZE("unsigned long long int"   UNSIGNED_LONG_LONG_INT)

#
CHECK_TYPE_SIZE(dev_t       DEV_T)
IF(NOT HAVE_DEV_T)
  IF(MSVC)
    SET(dev_t "unsigned int")
  ENDIF(MSVC)
ENDIF(NOT HAVE_DEV_T)
#
CHECK_TYPE_SIZE(gid_t       GID_T)
IF(NOT HAVE_GID_T)
  IF(WIN32)
    SET(gid_t "short")
  ELSE(WIN32)
    SET(gid_t "unsigned int")
  ENDIF(WIN32)
ENDIF(NOT HAVE_GID_T)
#
CHECK_TYPE_SIZE(id_t        ID_T)
IF(NOT HAVE_ID_T)
  IF(WIN32)
    SET(id_t "short")
  ELSE(WIN32)
    SET(id_t "unsigned int")
  ENDIF(WIN32)
ENDIF(NOT HAVE_ID_T)
#
CHECK_TYPE_SIZE(int32_t    INT32_T)
IF(NOT HAVE_INT32_T)
  SET(int32_t "int")
ENDIF(NOT HAVE_INT32_T)
#
CHECK_TYPE_SIZE(int64_t     INT64_T)
IF(NOT HAVE_INT64_T)
  IF(WIN32)
    SET(int64_t __int64)
  ENDIF(WIN32)
ENDIF(NOT HAVE_INT64_T)
#
CHECK_TYPE_SIZE(intmax_t    INTMAX_T)
IF(NOT HAVE_INTMAX_T)
  SET(intmax_t "int64_t")
ENDIF(NOT HAVE_INTMAX_T)
#
CHECK_TYPE_SIZE(mode_t      MODE_T)
IF(NOT HAVE_MODE_T)
  IF(WIN32)
    SET(mode_t "unsigned short")
  ELSE(WIN32)
    SET(mode_t "int")
  ENDIF(WIN32)
ENDIF(NOT HAVE_MODE_T)
#
CHECK_TYPE_SIZE(off_t       OFF_T)
IF(NOT HAVE_OFF_T)
  SET(off_t "__int64")
ENDIF(NOT HAVE_OFF_T)
#
CHECK_TYPE_SIZE(size_t      SIZE_T)
IF(NOT HAVE_SIZE_T)
  IF("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    SET(size_t "uint64_t")
  ELSE("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    SET(size_t   "uint32_t")
  ENDIF("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
ENDIF(NOT HAVE_SIZE_T)
#
CHECK_TYPE_SIZE(ssize_t     SSIZE_T)
IF(NOT HAVE_SSIZE_T)
  IF("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    SET(ssize_t "int64_t")
  ELSE("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    SET(ssize_t "long")
  ENDIF("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
ENDIF(NOT HAVE_SSIZE_T)
#
CHECK_TYPE_SIZE(uid_t       UID_T)
IF(NOT HAVE_UID_T)
  IF(WIN32)
    SET(uid_t "short")
  ELSE(WIN32)
    SET(uid_t "unsigned int")
  ENDIF(WIN32)
ENDIF(NOT HAVE_UID_T)
#
CHECK_TYPE_SIZE(pid_t       PID_T)
IF(NOT HAVE_PID_T)
  IF(WIN32)
    SET(pid_t "int")
  ELSE(WIN32)
    MESSAGE(FATAL_ERROR "pid_t doesn't exist on this platform?")
  ENDIF(WIN32)
ENDIF(NOT HAVE_PID_T)
#
CHECK_TYPE_SIZE(uint16_t    UINT16_T)
IF(NOT HAVE_UINT16_T)
  SET(uint16_t "unsigned short")
ENDIF(NOT HAVE_UINT16_T)
#
CHECK_TYPE_SIZE(uint32_t    UINT32_T)
IF(NOT HAVE_UINT32_T)
  SET(uint32_t "unsigned int")
ENDIF(NOT HAVE_UINT32_T)
#
CHECK_TYPE_SIZE(uint64_t    UINT64_T)
IF(NOT HAVE_UINT64_T)
  IF(WIN32)
    SET(uint64_t "unsigned __int64")
  ENDIF(WIN32)
ENDIF(NOT HAVE_UINT64_T)
#
CHECK_TYPE_SIZE(uintmax_t   UINTMAX_T)
IF(NOT HAVE_UINTMAX_T)
  SET(uintmax_t "uint64_t")
ENDIF(NOT HAVE_UINTMAX_T)
#
CHECK_TYPE_SIZE(intptr_t   INTPTR_T)
IF(NOT HAVE_INTPTR_T)
  IF("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    SET(intptr_t "int64_t")
  ELSE()
    SET(intptr_t "int32_t")
  ENDIF()
ENDIF(NOT HAVE_INTPTR_T)
#
CHECK_TYPE_SIZE(uintptr_t   UINTPTR_T)
IF(NOT HAVE_UINTPTR_T)
  IF("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
    SET(uintptr_t "uint64_t")
  ELSE()
    SET(uintptr_t "uint32_t")
  ENDIF()
ENDIF(NOT HAVE_UINTPTR_T)
#
CHECK_TYPE_SIZE(wchar_t     SIZEOF_WCHAR_T)
IF(HAVE_SIZEOF_WCHAR_T)
  SET(HAVE_WCHAR_T 1)
ENDIF(HAVE_SIZEOF_WCHAR_T)
#
# Check if _FILE_OFFSET_BITS macro needed for large files
#
CHECK_FILE_OFFSET_BITS()


# Generate "config.h" from "build/cmake/config.h.in"
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/build/cmake/config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/config.h)
INCLUDE_DIRECTORIES(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
ADD_DEFINITIONS(-DHAVE_CONFIG_H)

#
# Register installation of PDF documents.
#
IF(WIN32 AND NOT CYGWIN)
  #
  # On Windows platform, It's better that we install PDF documents
  # on one's computer.
  # These PDF documents are available in the release package.
  #
  IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/doc/pdf)
    INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doc/pdf
            DESTINATION share/man
            FILES_MATCHING PATTERN "*.pdf"
    )
  ENDIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/doc/pdf)
ENDIF(WIN32 AND NOT CYGWIN)
#
#
#
INCLUDE_DIRECTORIES(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/libtransform)
#
IF(MSVC)
  ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE)
ENDIF(MSVC)
# Especially for early development, we want to be a little
# aggressive about diagnosing build problems; this can get
# relaxed somewhat in final shipping versions.
IF ("CMAKE_C_COMPILER_ID" MATCHES "^GNU$")
  ADD_DEFINITIONS(-Wall -Werror)
ENDIF ("CMAKE_C_COMPILER_ID" MATCHES "^GNU$")

IF(ENABLE_TEST)
ADD_CUSTOM_TARGET(run_all_tests)
ENDIF(ENABLE_TEST)

add_subdirectory(libtransform)
add_subdirectory(tar)
add_subdirectory(cpio)
