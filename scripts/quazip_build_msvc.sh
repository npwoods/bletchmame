#!/bin/bash

set -euo pipefail

###################################################################################
# quazip_build_msvc.sh - Builds QuaZip for MSVC                                   #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

CONFIG=Debug
MSVC_VER=msvc2019

# QT version is 6.1.2 by default
QT_VERSION=6.1.2

# parse arguments
while getopts "c:q:t:v:" OPTION; do
   case $OPTION in
      c)
         CONFIG=$OPTARG
		 ;;
      q)
         QT_VERSION=$OPTARG
		 ;;
      t)
         TOOLSET=$OPTARG
		 ;;
      v)
         MSVC_VER=$OPTARG
         ;;
   esac
done

if [ -z "${QT6_INSTALL_DIR:-}" ]; then
  QT6_INSTALL_DIR=/c/Qt/${QT_VERSION}/msvc2019_64
fi
if [ ! -d "$QT6_INSTALL_DIR" ]; then
  echo "Bad QT6_INSTALL_DIR"
  exit
fi

# Identify directories
QUAZIP_DIR=$(dirname $BASH_SOURCE)/../deps/quazip
QUAZIP_BUILD_DIR=$(dirname $BASH_SOURCE)/../deps/build/$MSVC_VER/quazip
INSTALL_DIR=$(realpath $(dirname $BASH_SOURCE)/../deps/install/$MSVC_VER)

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

if [ "$CONFIG" = Debug ]; then
  ZLIB_LIBNAME=zlibstaticd.lib
else
  ZLIB_LIBNAME=zlibstatic.lib
fi

# Build and install it!
rm -rf $QUAZIP_BUILD_DIR
cmake -S$QUAZIP_DIR -B$QUAZIP_BUILD_DIR -DQUAZIP_QT_MAJOR_VERSION=6	\
	--install-prefix $INSTALL_DIR									\
	-G"$GENERATOR"	            									\
	-DQt6_DIR=$QT6_INSTALL_DIR/lib/cmake/Qt6						\
	-DQt6Core_DIR=$QT6_INSTALL_DIR/lib/cmake/Qt6Core				\
	-DQt6CoreTools_DIR=$QT6_INSTALL_DIR/lib/cmake/Qt6CoreTools		\
	-DZLIB_LIBRARY=$INSTALL_DIR/lib/${ZLIB_LIBNAME}					\
	-DZLIB_INCLUDE_DIR=$INSTALL_DIR/include
cmake --build $QUAZIP_BUILD_DIR --parallel --config $CONFIG
cmake --install $QUAZIP_BUILD_DIR --config $CONFIG
