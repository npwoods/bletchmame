#!/bin/bash

set -euo pipefail

###################################################################################
# liblzma_build_msvc.sh - Builds Liblzma for MSVC                                 #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

CONFIG=Debug
MSVC_VER=msvc2022

# parse arguments
while getopts "c:t:v:" OPTION; do
   case $OPTION in
      c)
         CONFIG=$OPTARG
		 ;;
      t)
         TOOLSET=$OPTARG
		 ;;
      v)
         MSVC_VER=$OPTARG
         ;;
   esac
done

# Identify directories
LIBLZMA_DIR=$(dirname $BASH_SOURCE)/../deps/liblzma
LIBLZMA_BUILD_DIR=$(dirname $BASH_SOURCE)/../deps/build/$MSVC_VER/liblzma
INSTALL_DIR=$(dirname $BASH_SOURCE)/../deps/install/$MSVC_VER

# Make directories absolute
mkdir -p $INSTALL_DIR
INSTALL_DIR=$(realpath $INSTALL_DIR)

# MSVC versions
case $MSVC_VER in
    msvc2019)
        GENERATOR="Visual Studio 16 2019"
        ;;
    msvc2022)
        GENERATOR="Visual Studio 17 2022"
        ;;
    *)
        echo "Unknown MSVC version $MSVC_VER"
        exit 1
        ;;
esac

# Build and install it!
rm -rf $LIBLZMA_BUILD_DIR
cmake -S$LIBLZMA_DIR -B$LIBLZMA_BUILD_DIR							\
	--install-prefix $INSTALL_DIR									\
	-G"$GENERATOR"
cmake --build $LIBLZMA_BUILD_DIR --parallel --config $CONFIG
cmake --install $LIBLZMA_BUILD_DIR --config $CONFIG
