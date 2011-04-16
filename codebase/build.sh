#!/bin/sh

NEWROOT=project1-14
cd pf; make clean; cd ..
cd rm; make clean; cd ..

rm -rf "$NEWROOT"
rm $NEWROOT.zip
mkdir -p "$NEWROOT"/codebase
cp -r pf "$NEWROOT"/codebase
cp -r rm "$NEWROOT"/codebase
cp project1-report makefile.inc readme.txt "$NEWROOT"/codebase
zip -r $NEWROOT.zip $NEWROOT
rm -rf "$NEWROOT"
