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
QT_DIR=$(dirname $BASH_SOURCE)/../deps/qt6
QT_BUILD_DIR=$(dirname $BASH_SOURCE)/../deps/qt6_msys2_build
QT_INSTALL_DIR=$(dirname $BASH_SOURCE)/../deps/qt6_msys2_install

# Clean directories
rm -rf $QT_BUILD_DIR $QT_INSTALL_DIR
mkdir -p $QT_BUILD_DIR
mkdir -p $QT_INSTALL_DIR

# Configure Qt
pushd $QT_BUILD_DIR
../qt6/configure.bat -release -static -static-runtime -prefix ../qt6_msys2_install -platform win32-g++ -opensource -confirm-license -qt-zlib -qt-libpng -qt-webp -qt-libjpeg -qt-freetype -no-opengl -make libs -nomake examples -nomake tests -skip qtmultimedia

# Build!
cmake --build . --parallel

# Install!
cmake --install .
