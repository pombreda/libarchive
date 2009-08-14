#!/bin/sh

mkdir "top"
pushd "top"

n=100
while [ $n -gt 0 ]; do
    d="abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
    mkdir $d
    cd $d
    n=$(($n - 1))

    m=0
    while [ $m -lt 100 ]; do
	touch $m
	m=$(($m + 1))
    done
    echo $n
done
popd

echo TREE
/usr/bin/time /bin/sh -c '../tree top >/dev/null' 2>&1
echo FIND
/usr/bin/time /bin/sh -c 'find top >/dev/null 2>&1' 2>&1
echo TREE
/usr/bin/time /bin/sh -c '../tree top >/dev/null' 2>&1
echo FIND
/usr/bin/time /bin/sh -c 'find top >/dev/null 2>&1' 2>&1
echo TREE
/usr/bin/time /bin/sh -c '../tree top >/dev/null' 2>&1
echo FIND
/usr/bin/time /bin/sh -c 'find top >/dev/null 2>&1' 2>&1
echo TREE
/usr/bin/time /bin/sh -c '../tree top >/dev/null' 2>&1
echo FIND
/usr/bin/time /bin/sh -c 'find top >/dev/null 2>&1' 2>&1

echo
echo RM -RF
/usr/bin/time rm -rf top
