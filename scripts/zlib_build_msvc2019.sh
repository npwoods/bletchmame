#!/bin/bash

set -euo pipefail

###################################################################################
# zlib_build_msvc2019.sh - Builds Zlib for MSVC                                   #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

CONFIG=Debug

# parse arguments
while getopts "c:" OPTION; do
   case $OPTION in
      c)
         CONFIG=$OPTARG
         ;;
   esac
done

# Identify directories
ZLIB_DIR=$(dirname $BASH_SOURCE)/../deps/zlib
ZLIB_BUILD_DIR=$(dirname $BASH_SOURCE)/../deps/build/msvc2019/zlib
INSTALL_DIR=$(realpath $(dirname $BASH_SOURCE)/../deps/msvc2019)

# Build and install it!
rm -rf $ZLIB_BUILD_DIR
cmake -S$ZLIB_DIR -B$ZLIB_BUILD_DIR		\
	--install-prefix $INSTALL_DIR		\
	-G"Visual Studio 16 2019"
cmake --build $ZLIB_BUILD_DIR --parallel --config $CONFIG
cmake --install $ZLIB_BUILD_DIR --config $CONFIG
