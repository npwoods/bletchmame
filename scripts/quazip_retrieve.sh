#!/bin/bash

###################################################################################
# quazip_retrieve.sh - Retrieves QuaZip sources                                   #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

# Retrieval of QuaZip
QUAZIP_DIR=$(dirname $BASH_SOURCE)/../deps/quazip
rm -rf $QUAZIP_DIR
git clone -b v1.1 --depth 1 git@github.com:stachenov/quazip.git $QUAZIP_DIR