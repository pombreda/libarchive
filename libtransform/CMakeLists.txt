
############################################
#
# How to build libtransform
#
############################################

# Public headers
SET(include_HEADERS
  transform.h
)

# Sources and private headers
SET(libtransform_SOURCES
  transform_check_magic.c
  transform_endian.h
  transform_platform.h
  transform_private.h
  transform_read.c
  transform_read_open_fd.c
  transform_read_open_file.c
  transform_read_open_memory.c
  transform_read_private.h
  filters/transform_read_all.c
  filters/transform_read_bzip2.c
  filters/transform_read_compress.c
  filters/transform_read_gzip.c
  filters/transform_read_none.c
  filters/transform_read_program.c
  filters/transform_read_rpm.c
  filters/transform_read_uu.c
  filters/transform_read_xz.c
  transform_string.c
  transform_string.h
  transform_string_sprintf.c
  transform_util.c
  transform_virtual.c
  transform_write.c
  transform_write_private.h
  transform_write_open_fd.c
  transform_write_open_file.c
  transform_write_open_memory.c
  filters/transform_write_bzip2.c
  filters/transform_write_compress.c
  filters/transform_write_gzip.c
  filters/transform_write_none.c
  filters/transform_write_program.c
  filters/transform_write_xz.c
  transform_write_set_options.c
  filters/filter_fork.c
  filters/filter_fork.h
)

# Man pages
SET(libtransform_MANS
  transform_util.3
  transform_write_set_options.3
  libtransform.3
  libtransform_internals.3
)

IF(WIN32 AND NOT CYGWIN)
  LIST(APPEND libtransform_SOURCES transform_windows.c)
  LIST(APPEND libtransform_SOURCES transform_windows.h)
  LIST(APPEND libtransform_SOURCES filters/filter_fork_windows.c)
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
        ARCHIVE DESTINATION lib)
INSTALL_MAN(${libtransform_MANS})
INSTALL(FILES ${include_HEADERS} DESTINATION include)

#add_subdirectory(test)
