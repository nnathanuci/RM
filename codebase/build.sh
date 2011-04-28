#!/bin/sh

NEWROOT=project1-14

git clean -f

cd pf; gmake clean; cd ..
cd rm; gmake clean; cd ..

rm -rf "$NEWROOT"
rm $NEWROOT.zip
mkdir -p "$NEWROOT"/codebase
cp -r pf "$NEWROOT"/codebase
cp -r rm "$NEWROOT"/codebase
cp project1-report makefile.inc readme.txt "$NEWROOT"/codebase
zip -r $NEWROOT.zip $NEWROOT
rm -rf "$NEWROOT"
