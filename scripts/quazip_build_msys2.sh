#!/bin/bash

###################################################################################
# qt_build_msys2.sh - Builds Qt for MSYS2                                         #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

# Identify directories
QUAZIP_DIR=$(dirname $BASH_SOURCE)/../deps/quazip
QUAZIP_BUILD_DIR=$(dirname $BASH_SOURCE)/../deps/quazip_msys2_build
QUAZIP_INSTALL_DIR=$(dirname $BASH_SOURCE)/../deps/quazip_msys2_install
QT_INSTALL_DIR=$(dirname $BASH_SOURCE)/../deps/qt6_msys2_install

# Build and install it!
rm -rf $QUAZIP_BUILD_DIR $QUAZIP_INSTALL_DIR
ls -l $QT_INSTALL_DIR/lib/cmake/Qt6/Qt6Config.cmake
cmake -S$QUAZIP_DIR -B$QUAZIP_BUILD_DIR -DBUILD_SHARED_LIBS=off -DQUAZIP_QT_MAJOR_VERSION=6 -DQt6_DIR=../qt6_msys2_install/lib/cmake/Qt6 -DQt6Core_DIR=../qt6_msys2_install/lib/cmake/Qt6Core -DQt6CoreTools_DIR=../qt6_msys2_install/lib/cmake/Qt6CoreTools
cmake --build $QUAZIP_BUILD_DIR --parallel
cmake --install $QUAZIP_BUILD_DIR --prefix $QUAZIP_INSTALL_DIR