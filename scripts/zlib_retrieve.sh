#!/bin/bash

###################################################################################
# zlib_retrieve.sh - Retrieves Zlib sources                                       #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

# Retrieval of Zlib
ZLIB_DIR=$(dirname $BASH_SOURCE)/../deps/zlib
rm -rf $ZLIB_DIR
git clone -b v1.2.11 --depth 1 https://github.com/madler/zlib.git $ZLIB_DIR
