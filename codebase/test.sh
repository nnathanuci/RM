#!/bin/sh

set -vx
unzip $1.zip
cd $1
cd codebase
cd pf
gmake clean
gmake
cd ../rm
gmake clean
gmake
./rmtest
