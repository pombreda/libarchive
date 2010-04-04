## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS= foreign subdir-objects
ACLOCAL_AMFLAGS = -I build/autoconf

#
# What to build and install
#
lib_LTLIBRARIES=	libarchive.la libstransform.la
noinst_LTLIBRARIES=	libarchive_fe.la
man_MANS= $(libarchive_man_MANS)
BUILT_SOURCES= libarchive/test/list.h

#
# What to test: We always test libarchive, test bsdtar and bsdcpio only
# if we built them.
#
check_PROGRAMS= libarchive_test
TESTS= libarchive_test
TESTS_ENVIRONMENT= $(libarchive_TESTS_ENVIRONMENT)
# Always build and test both bsdtar and bsdcpio as part of 'distcheck'
# The next line is commented out by default in shipping libarchive releases.
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
	build/windows			\
	contrib				\
	doc				\
	examples			\
	$(libarchive_EXTRA_DIST)	\
	$(libarchive_test_EXTRA_DIST)

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
	libarchive/test/list.h

distclean-local:
	-rm -rf .ref
	-rm -rf autom4te.cache/
	-rm -f *~
	-[ -f libarchive/Makefile ] && cd libarchive && make clean
	-[ -f libarchive/test/Makefile ] && cd libarchive/test && make clean

#
# Libarchive headers, source, etc.
#
#

include_HEADERS= libarchive/archive.h libarchive/archive_entry.h

libarchive_la_SOURCES=						\
	libarchive/archive_check_magic.c			\
	libarchive/archive_crc32.h				\
	libarchive/archive_endian.h				\
	libarchive/archive_entry.c				\
	libarchive/archive_entry.h				\
	libarchive/archive_entry_copy_stat.c			\
	libarchive/archive_entry_link_resolver.c		\
	libarchive/archive_entry_private.h			\
	libarchive/archive_entry_sparse.c			\
	libarchive/archive_entry_stat.c				\
	libarchive/archive_entry_strmode.c			\
	libarchive/archive_entry_xattr.c			\
	libarchive/archive_hash.h				\
	libarchive/archive_platform.h				\
	libarchive/archive_private.h				\
	libarchive/archive_read.c				\
	libarchive/archive_read_data_into_fd.c			\
	libarchive/archive_read_disk.c				\
	libarchive/archive_read_disk_entry_from_file.c		\
	libarchive/archive_read_disk_private.h			\
	libarchive/archive_read_disk_set_standard_lookup.c	\
	libarchive/archive_read_extract.c			\
	libarchive/archive_read_open_fd.c			\
	libarchive/archive_read_open_file.c			\
	libarchive/archive_read_open_filename.c			\
	libarchive/archive_read_open_memory.c			\
	libarchive/archive_read_private.h			\
	libarchive/archive_read_support_format_all.c		\
	libarchive/archive_read_support_format_ar.c		\
	libarchive/archive_read_support_format_cpio.c		\
	libarchive/archive_read_support_format_empty.c		\
	libarchive/archive_read_support_format_iso9660.c	\
	libarchive/archive_read_support_format_mtree.c		\
	libarchive/archive_read_support_format_raw.c		\
	libarchive/archive_read_support_format_tar.c		\
	libarchive/archive_read_support_format_xar.c		\
	libarchive/archive_read_support_format_zip.c		\
	libarchive/archive_string.c				\
	libarchive/archive_string.h				\
	libarchive/archive_string_sprintf.c			\
	libarchive/archive_util.c				\
	libarchive/archive_virtual.c				\
	libarchive/archive_write.c				\
	libarchive/archive_write_disk.c				\
	libarchive/archive_write_disk_private.h			\
	libarchive/archive_write_disk_set_standard_lookup.c	\
	libarchive/archive_write_open_fd.c			\
	libarchive/archive_write_open_file.c			\
	libarchive/archive_write_open_filename.c		\
	libarchive/archive_write_open_memory.c			\
	libarchive/archive_write_private.h			\
	libarchive/archive_write_set_format.c			\
	libarchive/archive_write_set_format_ar.c		\
	libarchive/archive_write_set_format_by_name.c		\
	libarchive/archive_write_set_format_cpio.c		\
	libarchive/archive_write_set_format_cpio_newc.c		\
	libarchive/archive_write_set_format_iso9660.c		\
	libarchive/archive_write_set_format_mtree.c		\
	libarchive/archive_write_set_format_pax.c		\
	libarchive/archive_write_set_format_shar.c		\
	libarchive/archive_write_set_format_ustar.c		\
	libarchive/archive_write_set_format_zip.c		\
	libarchive/archive_write_set_options.c			\
	libarchive/config_freebsd.h

libarchive_la_LIBADD = libstransform.la

libstransform_la_SOURCES = 									\
	libarchive/archive_read_support_compression_all.c	\
	libarchive/archive_read_support_compression_bzip2.c	\
	libarchive/archive_read_support_compression_compress.c	\
	libarchive/archive_read_support_compression_gzip.c	\
	libarchive/archive_read_support_compression_none.c	\
	libarchive/archive_read_support_compression_program.c	\
	libarchive/archive_read_support_compression_rpm.c	\
	libarchive/archive_read_support_compression_uu.c	\
	libarchive/archive_read_support_compression_xz.c	\
	libarchive/archive_write_add_filter_bzip2.c	\
	libarchive/archive_write_add_filter_compress.c	\
	libarchive/archive_write_add_filter_gzip.c		\
	libarchive/archive_write_add_filter_none.c		\
	libarchive/archive_write_add_filter_program.c	\
	libarchive/archive_write_add_filter_xz.c		\
	libarchive/filter_fork.c						\
	libarchive/filter_fork.h

if INC_WINDOWS_FILES
libarchive_la_SOURCES+=						\
	libarchive/archive_entry_copy_bhfi.c			\
	libarchive/archive_windows.h				\
	libarchive/archive_windows.c				\
	libarchive/filter_fork_windows.c
endif

# -no-undefined marks that libarchive doesn't rely on symbols
# defined in the application.  This is mandatory for cygwin.
libarchive_la_LDFLAGS= -no-undefined -version-info $(ARCHIVE_LIBTOOL_VERSION)
libstransform_la_LDFLAGS= -no-undefined

# Manpages to install
libarchive_man_MANS=						\
	libarchive/archive_entry.3				\
	libarchive/archive_entry_acl.3				\
	libarchive/archive_entry_linkify.3			\
	libarchive/archive_entry_paths.3			\
	libarchive/archive_entry_perms.3			\
	libarchive/archive_entry_stat.3				\
	libarchive/archive_entry_time.3				\
	libarchive/archive_read.3				\
	libarchive/archive_read_disk.3				\
	libarchive/archive_util.3				\
	libarchive/archive_write.3				\
	libarchive/archive_write_disk.3				\
	libarchive/archive_write_set_options.3			\
	libarchive/cpio.5					\
	libarchive/libarchive.3					\
	libarchive/libarchive_internals.3			\
	libarchive/libarchive-formats.5				\
	libarchive/mtree.5					\
	libarchive/tar.5

# Additional libarchive files to include in the distribution
libarchive_EXTRA_DIST=			\
	libarchive/test/list.h		\
	libarchive/archive_windows.c	\
	libarchive/archive_windows.h	\
	libarchive/filter_fork_windows.c	\
	libarchive/CMakeLists.txt	\
	$(libarchive_man_MANS)

# pkgconfig
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = build/pkgconfig/libarchive.pc

#
#
# libarchive_test program
#
#
libarchive_test_SOURCES=					\
	$(libarchive_la_SOURCES)				\
	libarchive/test/main.c					\
	libarchive/test/read_open_memory.c			\
	libarchive/test/test.h					\
	libarchive/test/test_acl_basic.c			\
	libarchive/test/test_acl_freebsd.c			\
	libarchive/test/test_acl_pax.c				\
	libarchive/test/test_archive_api_feature.c		\
	libarchive/test/test_bad_fd.c				\
	libarchive/test/test_compat_bzip2.c			\
	libarchive/test/test_compat_cpio.c			\
	libarchive/test/test_compat_gtar.c			\
	libarchive/test/test_compat_gzip.c			\
	libarchive/test/test_compat_lzma.c			\
	libarchive/test/test_compat_solaris_tar_acl.c		\
	libarchive/test/test_compat_tar_hardlink.c		\
	libarchive/test/test_compat_xz.c			\
	libarchive/test/test_compat_zip.c			\
	libarchive/test/test_empty_write.c			\
	libarchive/test/test_entry.c				\
	libarchive/test/test_extattr_freebsd.c			\
	libarchive/test/test_fuzz.c				\
	libarchive/test/test_entry_strmode.c			\
	libarchive/test/test_link_resolver.c			\
	libarchive/test/test_open_fd.c				\
	libarchive/test/test_open_file.c			\
	libarchive/test/test_open_filename.c			\
	libarchive/test/test_pax_filename_encoding.c		\
	libarchive/test/test_read_compress_program.c		\
	libarchive/test/test_read_data_large.c			\
	libarchive/test/test_read_disk.c			\
	libarchive/test/test_read_disk_entry_from_file.c	\
	libarchive/test/test_read_extract.c			\
	libarchive/test/test_read_file_nonexistent.c		\
	libarchive/test/test_read_format_ar.c			\
	libarchive/test/test_read_format_cpio_afio.c		\
	libarchive/test/test_read_format_cpio_bin.c		\
	libarchive/test/test_read_format_cpio_bin_Z.c		\
	libarchive/test/test_read_format_cpio_bin_be.c		\
	libarchive/test/test_read_format_cpio_bin_bz2.c		\
	libarchive/test/test_read_format_cpio_bin_gz.c		\
	libarchive/test/test_read_format_cpio_bin_lzma.c	\
	libarchive/test/test_read_format_cpio_bin_xz.c		\
	libarchive/test/test_read_format_cpio_odc.c		\
	libarchive/test/test_read_format_cpio_svr4_bzip2_rpm.c	\
	libarchive/test/test_read_format_cpio_svr4_gzip.c	\
	libarchive/test/test_read_format_cpio_svr4_gzip_rpm.c	\
	libarchive/test/test_read_format_cpio_svr4c_Z.c		\
	libarchive/test/test_read_format_empty.c		\
	libarchive/test/test_read_format_gtar_gz.c		\
	libarchive/test/test_read_format_gtar_lzma.c		\
	libarchive/test/test_read_format_gtar_sparse.c		\
	libarchive/test/test_read_format_iso_gz.c		\
	libarchive/test/test_read_format_iso_multi_extent.c	\
	libarchive/test/test_read_format_isojoliet_bz2.c	\
	libarchive/test/test_read_format_isojoliet_long.c	\
	libarchive/test/test_read_format_isojoliet_rr.c		\
	libarchive/test/test_read_format_isorr_bz2.c		\
	libarchive/test/test_read_format_isorr_ce.c		\
	libarchive/test/test_read_format_isorr_new_bz2.c	\
	libarchive/test/test_read_format_isorr_rr_moved.c	\
	libarchive/test/test_read_format_isozisofs_bz2.c	\
	libarchive/test/test_read_format_mtree.c		\
	libarchive/test/test_read_format_pax_bz2.c		\
	libarchive/test/test_read_format_raw.c			\
	libarchive/test/test_read_format_tar.c			\
	libarchive/test/test_read_format_tar_empty_filename.c	\
	libarchive/test/test_read_format_tbz.c			\
	libarchive/test/test_read_format_tgz.c			\
	libarchive/test/test_read_format_tlz.c			\
	libarchive/test/test_read_format_txz.c			\
	libarchive/test/test_read_format_tz.c			\
	libarchive/test/test_read_format_xar.c			\
	libarchive/test/test_read_format_zip.c			\
	libarchive/test/test_read_large.c			\
	libarchive/test/test_read_pax_truncated.c		\
	libarchive/test/test_read_position.c			\
	libarchive/test/test_read_truncated.c			\
	libarchive/test/test_read_uu.c				\
	libarchive/test/test_sparse_basic.c			\
	libarchive/test/test_tar_filenames.c			\
	libarchive/test/test_tar_large.c			\
	libarchive/test/test_ustar_filenames.c			\
	libarchive/test/test_write_compress.c			\
	libarchive/test/test_write_compress_bzip2.c		\
	libarchive/test/test_write_compress_gzip.c		\
	libarchive/test/test_write_compress_lzma.c		\
	libarchive/test/test_write_compress_program.c		\
	libarchive/test/test_write_compress_xz.c		\
	libarchive/test/test_write_disk.c			\
	libarchive/test/test_write_disk_failures.c		\
	libarchive/test/test_write_disk_hardlink.c		\
	libarchive/test/test_write_disk_perms.c			\
	libarchive/test/test_write_disk_secure.c		\
	libarchive/test/test_write_disk_sparse.c		\
	libarchive/test/test_write_disk_symlink.c		\
	libarchive/test/test_write_disk_times.c			\
	libarchive/test/test_write_format_ar.c			\
	libarchive/test/test_write_format_cpio.c		\
	libarchive/test/test_write_format_cpio_empty.c		\
	libarchive/test/test_write_format_cpio_odc.c		\
	libarchive/test/test_write_format_cpio_newc.c		\
	libarchive/test/test_write_format_iso9660.c		\
	libarchive/test/test_write_format_iso9660_boot.c	\
	libarchive/test/test_write_format_iso9660_empty.c	\
	libarchive/test/test_write_format_iso9660_filename.c	\
	libarchive/test/test_write_format_iso9660_zisofs.c	\
	libarchive/test/test_write_format_mtree.c		\
	libarchive/test/test_write_format_pax.c			\
	libarchive/test/test_write_format_shar_empty.c		\
	libarchive/test/test_write_format_tar.c			\
	libarchive/test/test_write_format_tar_empty.c		\
	libarchive/test/test_write_format_tar_ustar.c		\
	libarchive/test/test_write_format_zip.c			\
	libarchive/test/test_write_format_zip_empty.c		\
	libarchive/test/test_write_format_zip_no_compression.c	\
	libarchive/test/test_write_open_memory.c

libarchive_test_CPPFLAGS= -I$(top_srcdir)/libarchive -I$(top_builddir)/libarchive/test -DLIBARCHIVE_STATIC $(PLATFORMCPPFLAGS)

# The "list.h" file just lists all of the tests defined in all of the sources.
# Building it automatically provides a sanity-check on libarchive_test_SOURCES
# above.
libarchive/test/list.h: Makefile
	cat $(top_srcdir)/libarchive/test/test_*.c | grep DEFINE_TEST > libarchive/test/list.h

libarchive_TESTS_ENVIRONMENT= LIBARCHIVE_TEST_FILES=`cd $(top_srcdir);/bin/pwd`/libarchive/test

libarchive_test_EXTRA_DIST=\
	libarchive/test/test_compat_bzip2_1.tbz.uu			\
	libarchive/test/test_compat_bzip2_2.tbz.uu			\
	libarchive/test/test_compat_cpio_1.cpio.uu			\
	libarchive/test/test_compat_gtar_1.tar.uu			\
	libarchive/test/test_compat_gzip_1.tgz.uu			\
	libarchive/test/test_compat_gzip_2.tgz.uu			\
	libarchive/test/test_compat_lzma_1.tlz.uu			\
	libarchive/test/test_compat_lzma_2.tlz.uu			\
	libarchive/test/test_compat_lzma_3.tlz.uu			\
	libarchive/test/test_compat_solaris_tar_acl.tar.uu		\
	libarchive/test/test_compat_tar_hardlink_1.tar.uu		\
	libarchive/test/test_compat_xz_1.txz.uu				\
	libarchive/test/test_compat_zip_1.zip.uu			\
	libarchive/test/test_fuzz_1.iso.Z.uu				\
	libarchive/test/test_pax_filename_encoding.tar.uu		\
	libarchive/test/test_read_format_ar.ar.uu			\
	libarchive/test/test_read_format_cpio_bin_be.cpio.uu		\
	libarchive/test/test_read_format_cpio_svr4_bzip2_rpm.rpm.uu	\
	libarchive/test/test_read_format_cpio_svr4_gzip_rpm.rpm.uu	\
	libarchive/test/test_read_format_gtar_sparse_1_13.tar.uu	\
	libarchive/test/test_read_format_gtar_sparse_1_17.tar.uu	\
	libarchive/test/test_read_format_gtar_sparse_1_17_posix00.tar.uu \
	libarchive/test/test_read_format_gtar_sparse_1_17_posix01.tar.uu \
	libarchive/test/test_read_format_gtar_sparse_1_17_posix10.tar.uu \
	libarchive/test/test_read_format_gtar_sparse_1_17_posix10_modified.tar.uu \
	libarchive/test/test_read_format_iso.iso.Z.uu			\
	libarchive/test/test_read_format_iso_joliet.iso.Z.uu		\
	libarchive/test/test_read_format_iso_joliet_long.iso.Z.uu	\
	libarchive/test/test_read_format_iso_joliet_rockridge.iso.Z.uu	\
	libarchive/test/test_read_format_iso_multi_extent.iso.Z.uu	\
	libarchive/test/test_read_format_iso_rockridge.iso.Z.uu		\
	libarchive/test/test_read_format_iso_rockridge_ce.iso.Z.uu	\
	libarchive/test/test_read_format_iso_rockridge_new.iso.Z.uu	\
	libarchive/test/test_read_format_iso_rockridge_rr_moved.iso.Z.uu\
	libarchive/test/test_read_format_iso_zisofs.iso.Z.uu		\
	libarchive/test/test_read_format_mtree.mtree.uu			\
	libarchive/test/test_read_format_raw.data.Z.uu			\
	libarchive/test/test_read_format_raw.data.uu			\
	libarchive/test/test_read_format_tar_empty_filename.tar.uu	\
	libarchive/test/test_read_format_zip.zip.uu			\
	libarchive/test/CMakeLists.txt					\
	libarchive/test/README

#
# Common code for libarchive frontends (cpio, tar)
#
libarchive_fe_la_SOURCES=			\
	libarchive_fe/err.c			\
	libarchive_fe/err.h			\
	libarchive_fe/lafe_platform.h		\
	libarchive_fe/line_reader.c		\
	libarchive_fe/line_reader.h		\
	libarchive_fe/matching.c		\
	libarchive_fe/matching.h		\
	libarchive_fe/pathmatch.c		\
	libarchive_fe/pathmatch.h
