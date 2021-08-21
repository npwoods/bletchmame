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
ZLIB_DIR=$(dirname $BASH_SOURCE)/../deps/zlib-ng
rm -rf $ZLIB_DIR
git clone -b 2.0.5 --depth 1 https://github.com/zlib-ng/zlib-ng.git $ZLIB_DIR
