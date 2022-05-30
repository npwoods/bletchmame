#!/bin/bash

set -euo pipefail

###################################################################################
# lzma_build_msys2.sh - Builds lzma for MSYS2                                     #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

# Identify directories
LZMA_DIR=$(dirname $BASH_SOURCE)/../deps/lzma-c-sdk
LZMA_BUILD_DIR=$(dirname $BASH_SOURCE)/../deps/build/msys2/lzma-c-sdk
INSTALL_DIR=$(dirname $BASH_SOURCE)/../deps/install/msys2

# Make directories absolute
mkdir -p $INSTALL_DIR
INSTALL_DIR=$(realpath $INSTALL_DIR)

# Build and install it!
rm -rf $LZMA_BUILD_DIR
cmake -S$LZMA_DIR -B$LZMA_BUILD_DIR							\
	--install-prefix $INSTALL_DIR
cmake --build $LZMA_BUILD_DIR --parallel
cmake --install $LZMA_BUILD_DIR
