#!/bin/sh

aclocal-1.10 || { echo "failed aclocal"; exit 1; };
libtoolize --automake -c -f || { echo "failed libtoolize"; exit 1; }
autoreconf --install || { echo "failed autoreconf"; exit 1; }
automake --add-missing || { echo "automake fialed"; exit 1; }

if [ -x ./test.sh ] ; then
	exec ./test.sh "$@"
fi
echo "finished"
