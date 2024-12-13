#/bin/bash

DN="$(dirname $0)"
rm -rf $DN/libs/*/ $DN/build/*
cd $DN
git submodule update --init --recursive
