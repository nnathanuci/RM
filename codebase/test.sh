#!/bin/bash
unzip $1.zip
cd $1
cd codebase
cd pf
make clean
make
cd ../rm
make clean
make
./rmtest
