
############################################
#
# How to build libtransform
#
############################################

# Public headers
SET(include_HEADERS
  transform.h
  transform_entry.h
)

# Sources and private headers
SET(libtransform_SOURCES
  transform_check_magic.c
  transform_endian.h
  transform_entry.c
  transform_entry.h
  transform_entry_copy_stat.c
  transform_entry_link_resolver.c
  transform_entry_private.h
  transform_entry_sparse.c
  transform_entry_stat.c
  transform_entry_strmode.c
  transform_entry_xattr.c
  transform_platform.h
  transform_private.h
  transform_read.c
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
  transform_string.c
  transform_string.h
  transform_string_sprintf.c
  transform_util.c
  transform_virtual.c
  transform_write.c
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
SET(libtransform_MANS
  transform_entry.3
  transform_entry_linkify.3
  transform_entry_paths.3
  transform_entry_perms.3
  transform_entry_stat.3
  transform_entry_time.3
  transform_read.3
  transform_read_disk.3
  transform_util.3
  transform_write.3
  transform_write_disk.3
  transform_write_set_options.3
  cpio.5
  libtransform.3
  libtransform_internals.3
  libtransform-formats.5
  mtree.5
  tar.5
)

IF(WIN32 AND NOT CYGWIN)
  LIST(APPEND libtransform_SOURCES transform_entry_copy_bhfi.c)
  LIST(APPEND libtransform_SOURCES transform_windows.c)
  LIST(APPEND libtransform_SOURCES transform_windows.h)
  LIST(APPEND libtransform_SOURCES filter_fork_windows.c)
ENDIF(WIN32 AND NOT CYGWIN)

# Libtransform is a shared library
ADD_LIBRARY(transform SHARED ${libtransform_SOURCES} ${include_HEADERS})
TARGET_LINK_LIBRARIES(transform ${ADDITIONAL_LIBS})
SET_TARGET_PROPERTIES(transform PROPERTIES SOVERSION ${SOVERSION})

# transform_static is a static library
ADD_LIBRARY(transform_static STATIC ${libtransform_SOURCES} ${include_HEADERS})
SET_TARGET_PROPERTIES(transform_static PROPERTIES COMPILE_DEFINITIONS
  LIBTRANSFORM_STATIC)
# On Posix systems, libtransform.so and libtransform.a can co-exist.
IF(NOT WIN32 OR CYGWIN)
  SET_TARGET_PROPERTIES(transform_static PROPERTIES OUTPUT_NAME transform)
ENDIF(NOT WIN32 OR CYGWIN)

# How to install the libraries
INSTALL(TARGETS transform transform_static
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        TRANSFORM DESTINATION lib)
INSTALL_MAN(${libtransform_MANS})
INSTALL(FILES ${include_HEADERS} DESTINATION include)

add_subdirectory(test)
