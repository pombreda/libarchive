
############################################
#
# How to build libarchive
#
############################################

# Public headers
SET(include_HEADERS
  transform.h
  archive_entry.h
)

# Sources and private headers
SET(libarchive_SOURCES
  archive_check_magic.c
  archive_endian.h
  archive_entry.c
  archive_entry.h
  archive_entry_copy_stat.c
  archive_entry_link_resolver.c
  archive_entry_private.h
  archive_entry_sparse.c
  archive_entry_stat.c
  archive_entry_strmode.c
  archive_entry_xattr.c
  archive_hash.h
  archive_platform.h
  archive_private.h
  archive_read.c
  transform_read_data_into_fd.c
  transform_read_disk.c
  transform_read_disk_entry_from_file.c
  transform_read_disk_private.h
  transform_read_disk_set_standard_lookup.c
  transform_read_extract.c
  transform_read_open_fd.c
  transform_read_open_file.c
  transform_read_open_filename.c
  transform_read_open_memory.c
  transform_read_private.h
  transform_read_support_compression_all.c
  transform_read_support_compression_bzip2.c
  transform_read_support_compression_compress.c
  transform_read_support_compression_gzip.c
  transform_read_support_compression_none.c
  transform_read_support_compression_program.c
  transform_read_support_compression_rpm.c
  transform_read_support_compression_uu.c
  transform_read_support_compression_xz.c
  transform_read_support_format_all.c
  transform_read_support_format_ar.c
  transform_read_support_format_cpio.c
  transform_read_support_format_empty.c
  transform_read_support_format_iso9660.c
  transform_read_support_format_mtree.c
  transform_read_support_format_raw.c
  transform_read_support_format_tar.c
  transform_read_support_format_xar.c
  transform_read_support_format_zip.c
  archive_string.c
  archive_string.h
  archive_string_sprintf.c
  archive_util.c
  archive_virtual.c
  archive_write.c
  transform_write_disk.c
  transform_write_disk_private.h
  transform_write_disk_set_standard_lookup.c
  transform_write_private.h
  transform_write_open_fd.c
  transform_write_open_file.c
  transform_write_open_filename.c
  transform_write_open_memory.c
  transform_write_add_filter_bzip2.c
  transform_write_add_filter_compress.c
  transform_write_add_filter_gzip.c
  transform_write_add_filter_none.c
  transform_write_add_filter_program.c
  transform_write_add_filter_xz.c
  transform_write_set_format.c
  transform_write_set_format_ar.c
  transform_write_set_format_by_name.c
  transform_write_set_format_cpio.c
  transform_write_set_format_cpio_newc.c
  transform_write_set_format_iso9660.c
  transform_write_set_format_mtree.c
  transform_write_set_format_pax.c
  transform_write_set_format_shar.c
  transform_write_set_format_ustar.c
  transform_write_set_format_zip.c
  transform_write_set_options.c
  filter_fork.c
  filter_fork.h
)

# Man pages
SET(libarchive_MANS
  archive_entry.3
  archive_entry_acl.3
  archive_entry_linkify.3
  archive_entry_paths.3
  archive_entry_perms.3
  archive_entry_stat.3
  archive_entry_time.3
  archive_read.3
  transform_read_disk.3
  archive_util.3
  archive_write.3
  transform_write_disk.3
  transform_write_set_options.3
  cpio.5
  libarchive.3
  libarchive_internals.3
  libarchive-formats.5
  mtree.5
  tar.5
)

IF(WIN32 AND NOT CYGWIN)
  LIST(APPEND libarchive_SOURCES archive_entry_copy_bhfi.c)
  LIST(APPEND libarchive_SOURCES archive_windows.c)
  LIST(APPEND libarchive_SOURCES archive_windows.h)
  LIST(APPEND libarchive_SOURCES filter_fork_windows.c)
ENDIF(WIN32 AND NOT CYGWIN)

# Libarchive is a shared library
ADD_LIBRARY(archive SHARED ${libarchive_SOURCES} ${include_HEADERS})
TARGET_LINK_LIBRARIES(archive ${ADDITIONAL_LIBS})
SET_TARGET_PROPERTIES(archive PROPERTIES SOVERSION ${SOVERSION})

# archive_static is a static library
ADD_LIBRARY(archive_static STATIC ${libarchive_SOURCES} ${include_HEADERS})
SET_TARGET_PROPERTIES(archive_static PROPERTIES COMPILE_DEFINITIONS
  LIBARCHIVE_STATIC)
# On Posix systems, libarchive.so and libarchive.a can co-exist.
IF(NOT WIN32 OR CYGWIN)
  SET_TARGET_PROPERTIES(archive_static PROPERTIES OUTPUT_NAME archive)
ENDIF(NOT WIN32 OR CYGWIN)

# How to install the libraries
INSTALL(TARGETS archive archive_static
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
INSTALL_MAN(${libarchive_MANS})
INSTALL(FILES ${include_HEADERS} DESTINATION include)

add_subdirectory(test)
