## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS= foreign subdir-objects
ACLOCAL_AMFLAGS = -I build/autoconf

#
# What to build and install
#
lib_LTLIBRARIES=	libtransform.la
man_MANS= $(libtransform_man_MANS)
BUILT_SOURCES= libtransform/test/list.h

#
# What to test: We always test libtransform
#
check_PROGRAMS= libtransform_test
TESTS= libtransform_test
TESTS_ENVIRONMENT= $(libtransform_TESTS_ENVIRONMENT)
# The next line is commented out by default in shipping libtransform releases.
# It is uncommented by default in trunk.
AM_CFLAGS=-Wall -Werror
PLATFORMCPPFLAGS = @PLATFORMCPPFLAGS@
AM_CPPFLAGS=$(PLATFORMCPPFLAGS)

#
# What to include in the distribution
#
EXTRA_DIST= \
	CMakeLists.txt			\
	build/autogen.sh		\
	build/bump-version.sh		\
	build/clean.sh			\
	build/cmake			\
	build/version			\
	contrib				\
	doc				\
	examples			\
	$(libtransform_EXTRA_DIST)	\
	$(libtransform_test_EXTRA_DIST)

# a) Clean out some unneeded files and directories
# b) Collect all documentation and format it for distribution.
dist-hook:
	rm -rf `find $(distdir) -name CVS -type d`
	rm -rf `find $(distdir) -name .svn -type d`
	rm -f `find $(distdir) -name '*~'`
	rm -f `find $(distdir) -name '*.out'`
	rm -f `find $(distdir) -name '*.core'`
	-rm -f $(distdir)/*/Makefile $(distdir)/*/*/Makefile
	cd $(distdir)/doc && /bin/sh update.sh

# Verify cmake builds as part of the acceptance
distcheck-hook:
	mkdir $(distdir)/_build/cmtest
	cd $(distdir)/_build/cmtest && cmake ../.. && make && make test
	rm -rf $(distdir)/_build/cmtest

#
# Extra rules for cleanup
#
DISTCLEANFILES=					\
	libtransform/test/list.h

distclean-local:
	-rm -rf .ref
	-rm -rf autom4te.cache/
	-rm -f *~
	-[ -f libtransform/Makefile ] && cd libtransform && make clean
	-[ -f libtransform/test/Makefile ] && cd libtransform/test && make clean

#
# Libtransform headers, source, etc.
#
#

include_HEADERS= libtransform/transform.h

libtransform_la_SOURCES = 									\
	libtransform/transform_read.c							\
	libtransform/transform_read_open_fd.c			\
	libtransform/transform_read_open_file.c			\
	libtransform/transform_read_open_filename.c			\
	libtransform/transform_read_open_memory.c			\
	libtransform/filters/transform_read_all.c	\
	libtransform/filters/transform_read_bzip2.c	\
	libtransform/filters/transform_read_compress.c	\
	libtransform/filters/transform_read_gzip.c	\
	libtransform/filters/transform_read_none.c	\
	libtransform/filters/transform_read_program.c	\
	libtransform/filters/transform_read_rpm.c	\
	libtransform/filters/transform_read_uu.c	\
	libtransform/filters/transform_read_xz.c	\
	libtransform/transform_write.c							\
	libtransform/transform_write_set_options.c		\
	libtransform/transform_write_open_fd.c			\
	libtransform/transform_write_open_file.c			\
	libtransform/transform_write_open_filename.c			\
	libtransform/transform_write_open_memory.c			\
	libtransform/filters/transform_write_bzip2.c	\
	libtransform/filters/transform_write_compress.c	\
	libtransform/filters/transform_write_gzip.c		\
	libtransform/filters/transform_write_none.c		\
	libtransform/filters/transform_write_program.c	\
	libtransform/filters/transform_write_xz.c		\
	libtransform/transform_util.c					\
	libtransform/transform_virtual.c				\
	libtransform/transform_check_magic.c			\
	libtransform/transform_string.c					\
	libtransform/transform_string.h					\
	libtransform/transform_string_sprintf.c					\
	libtransform/filters/filter_fork.c						\
	libtransform/filters/filter_fork.h				\
	libtransform/transform_crc32.h

libtransform_la_CFLAGS = -I libtransform/

if INC_WINDOWS_FILES
libtransform_la_SOURCES+=						\
	libtransform/filters/filter_fork_windows.c
endif

# -no-undefined marks that libtransform doesn't rely on symbols
# defined in the application.  This is mandatory for cygwin.
libtransform_la_LDFLAGS= -no-undefined

# Manpages to install
libtransform_man_MANS=						\
	libtransform/libtransform.3					\
	libtransform/libtransform_internals.3	\
	libtransform/transform_write_set_options.3

# Additional libtransform files to include in the distribution
libtransform_EXTRA_DIST=			\
	libtransform/test/list.h		\
	libtransform/transform_windows.c	\
	libtransform/transform_windows.h	\
	libtransform/filters/filter_fork_windows.c	\
	libtransform/CMakeLists.txt	\
	$(libtransform_man_MANS)

# pkgconfig
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = build/pkgconfig/libtransform.pc

#
#
# libtransform_test program
#
#
libtransform_test_SOURCES=					\
	$(libtransform_la_SOURCES)				\
	libtransform/test/main.c					\
	libtransform/test/test.h					\
	libtransform/test/test_util.c				\
	libtransform/test/read_open_memory.c			\
	libtransform/test/test_transform_api_feature.c		\
	libtransform/test/test_bad_fd.c				\
	libtransform/test/test_filter_count.c		\
	libtransform/test/test_read_uu.c				\
	libtransform/test/test_read_file_nonexistent.c	
#	libtransform/test/test_open_fd.c				\
#	libtransform/test/test_open_file.c			\
#	libtransform/test/test_open_filename.c			
#	libtransform/test/test_compat_bzip2.c			\
#	libtransform/test/test_compat_cpio.c			\
#	libtransform/test/test_compat_gtar.c			\
#	libtransform/test/test_compat_gzip.c			\
#	libtransform/test/test_compat_lzma.c			\
#	libtransform/test/test_compat_lzip.c		\
#	libtransform/test/test_compat_xz.c			\
#	libtransform/test/test_compat_zip.c			\
#	libtransform/test/test_empty_write.c			\
#	libtransform/test/test_fuzz.c				\
#	libtransform/test/test_read_compress_program.c		\
#	libtransform/test/test_read_data_large.c			\
#	libtransform/test/test_read_disk.c			\
#	libtransform/test/test_read_truncated.c			\
#	libtransform/test/test_write_compress.c			\
#	libtransform/test/test_write_compress_bzip2.c		\
#	libtransform/test/test_write_compress_gzip.c		\
#	libtransform/test/test_write_compress_lzma.c		\
#	libtransform/test/test_write_compress_lzip.c		\
#	libtransform/test/test_write_compress_program.c		\
#	libtransform/test/test_write_compress_xz.c		\
#	libtransform/test/test_write_disk.c			\
#	libtransform/test/test_write_open_memory.c

libtransform_test_CPPFLAGS= -I$(top_srcdir)/libtransform -I$(top_builddir)/libtransform/test -DLIBTRANSFORM_STATIC $(PLATFORMCPPFLAGS)

# The "list.h" file just lists all of the tests defined in all of the sources.
# Building it automatically provides a sanity-check on libtransform_test_SOURCES
# above.
libtransform/test/list.h: Makefile
	cat $(top_srcdir)/libtransform/test/test_*.c | grep DEFINE_TEST > libtransform/test/list.h

libtransform_TESTS_ENVIRONMENT= LIBTRANSFORM_TEST_FILES=`cd $(top_srcdir);/bin/pwd`/libtransform/test

libtransform_test_EXTRA_DIST=\
	libtransform/test/test_compat_bzip2_1.tbz.uu			\
	libtransform/test/test_compat_bzip2_2.tbz.uu			\
	libtransform/test/test_compat_cpio_1.cpio.uu			\
	libtransform/test/test_compat_gtar_1.tar.uu			\
	libtransform/test/test_compat_gzip_1.tgz.uu			\
	libtransform/test/test_compat_gzip_2.tgz.uu			\
	libtransform/test/test_compat_lzma_1.tlz.uu			\
	libtransform/test/test_compat_lzma_2.tlz.uu			\
	libtransform/test/test_compat_lzma_3.tlz.uu			\
	libtransform/test/test_compat_solaris_tar_acl.tar.uu		\
	libtransform/test/test_compat_tar_hardlink_1.tar.uu		\
	libtransform/test/test_compat_xz_1.txz.uu				\
	libtransform/test/test_compat_zip_1.zip.uu			\
	libtransform/test/test_fuzz_1.iso.Z.uu				\
	libtransform/test/test_pax_filename_encoding.tar.uu		\
	libtransform/test/test_read_format_ar.ar.uu			\
	libtransform/test/test_read_format_cpio_bin_be.cpio.uu		\
	libtransform/test/test_read_format_cpio_svr4_bzip2_rpm.rpm.uu	\
	libtransform/test/test_read_format_cpio_svr4_gzip_rpm.rpm.uu	\
	libtransform/test/test_read_format_gtar_sparse_1_13.tar.uu	\
	libtransform/test/test_read_format_gtar_sparse_1_17.tar.uu	\
	libtransform/test/test_read_format_gtar_sparse_1_17_posix00.tar.uu \
	libtransform/test/test_read_format_gtar_sparse_1_17_posix01.tar.uu \
	libtransform/test/test_read_format_gtar_sparse_1_17_posix10.tar.uu \
	libtransform/test/test_read_format_gtar_sparse_1_17_posix10_modified.tar.uu \
	libtransform/test/test_read_format_iso.iso.Z.uu			\
	libtransform/test/test_read_format_iso_joliet.iso.Z.uu		\
	libtransform/test/test_read_format_iso_joliet_long.iso.Z.uu	\
	libtransform/test/test_read_format_iso_joliet_rockridge.iso.Z.uu	\
	libtransform/test/test_read_format_iso_multi_extent.iso.Z.uu	\
	libtransform/test/test_read_format_iso_rockridge.iso.Z.uu		\
	libtransform/test/test_read_format_iso_rockridge_ce.iso.Z.uu	\
	libtransform/test/test_read_format_iso_rockridge_new.iso.Z.uu	\
	libtransform/test/test_read_format_iso_rockridge_rr_moved.iso.Z.uu\
	libtransform/test/test_read_format_iso_zisofs.iso.Z.uu		\
	libtransform/test/test_read_format_mtree.mtree.uu			\
	libtransform/test/test_read_format_raw.data.Z.uu			\
	libtransform/test/test_read_format_raw.data.uu			\
	libtransform/test/test_read_format_tar_empty_filename.tar.uu	\
	libtransform/test/test_read_format_zip.zip.uu			\
	libtransform/test/CMakeLists.txt					\
	libtransform/test/README
