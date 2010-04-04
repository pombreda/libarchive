# TODO: This file should be moved into the build/cmake directory...

# The libtransform CDash page appears at
#   http://my.cdash.org/index.php?project=libtransform
set(CTEST_PROJECT_NAME "libtransform")
set(CTEST_NIGHTLY_START_TIME "01:00:00 UTC")

set(CTEST_DROP_METHOD "http")
set(CTEST_DROP_SITE "my.cdash.org")
set(CTEST_DROP_LOCATION "/submit.php?project=libtransform")
set(CTEST_DROP_SITE_CDASH TRUE)
