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
QT_DIR=$(realpath $(dirname $BASH_SOURCE)/../deps/qt6)
INSTALL_DIR=$(realpath $(dirname $BASH_SOURCE)/../deps/msys2)
QT_BUILD_DIR=$(realpath $(dirname $BASH_SOURCE)/../deps/msys2/build/qt6)

# Clean directories
rm -rf $QT_BUILD_DIR
mkdir -p $QT_BUILD_DIR

# Configure Qt
pushd $QT_BUILD_DIR
$QT_DIR/configure.bat -release -static -static-runtime -prefix $INSTALL_DIR -platform win32-g++ -opensource -confirm-license -qt-zlib -qt-libpng -qt-webp -qt-libjpeg -qt-freetype -qt-tiff -qt-pcre -no-jasper -no-opengl -no-mng -make libs -nomake examples -nomake tests -skip qtmultimedia

# Build!
cmake --build . --parallel

# Install!
cmake --install .
popd
