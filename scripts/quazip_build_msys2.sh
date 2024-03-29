#!/bin/bash

set -euo pipefail

###################################################################################
# quazip_build_msys2.sh - Builds QuaZip for MSYS2                                 #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

# Identify directories
QUAZIP_DIR=$(dirname $BASH_SOURCE)/../deps/quazip
INSTALL_DIR=$(dirname $BASH_SOURCE)/../deps/install/msys2
QUAZIP_BUILD_DIR=$(dirname $BASH_SOURCE)/../deps/build/msys2/quazip
QT_INSTALL_DIR=C:/msys64/mingw64/qt6-static

# parse arguments
BUILD_TYPE=Release					# can be Release/Debug/RelWithDebInfo etc
while getopts "b:" OPTION; do
   case $OPTION in
      b)
         BUILD_TYPE=$OPTARG
         ;;
   esac
done

# Clean directories
rm -rf $QUAZIP_BUILD_DIR
mkdir -p $QUAZIP_BUILD_DIR

# Make directories absolute
mkdir -p $INSTALL_DIR
QUAZIP_DIR=$(realpath $QUAZIP_DIR)
INSTALL_DIR=$(realpath $INSTALL_DIR)
QUAZIP_BUILD_DIR=$(realpath $QUAZIP_BUILD_DIR)

# link time optimization
if [ "$BUILD_TYPE" != "Debug" ]; then
LTO=TRUE
else
LTO=FALSE
fi

# Build and install it!
cmake -S$QUAZIP_DIR -B$QUAZIP_BUILD_DIR						\
	-DBUILD_SHARED_LIBS=off									\
	-DQUAZIP_QT_MAJOR_VERSION=6								\
	-DCMAKE_PREFIX_PATH="${INSTALL_DIR};${QT_INSTALL_DIR}"	\
	-DCMAKE_BUILD_TYPE=${BUILD_TYPE}						\
	-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=${LTO}
cmake --build $QUAZIP_BUILD_DIR --parallel
cmake --install $QUAZIP_BUILD_DIR --prefix $INSTALL_DIR
