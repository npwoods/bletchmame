#!/bin/bash

set -euo pipefail

###################################################################################
# qt6_build_msys2.sh - Builds Qt for MSYS2                                        #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

# Identify directories
QT_DIR=$(dirname $BASH_SOURCE)/../deps/qt6
INSTALL_DIR=$(dirname $BASH_SOURCE)/../deps/msys2
QT_BUILD_DIR=$(dirname $BASH_SOURCE)/../deps/build/msys2/qt6

# Clean directories
rm -rf $QT_BUILD_DIR
mkdir -p $QT_BUILD_DIR

# Make directories absolute
QT_DIR=$(realpath $QT_DIR)
INSTALL_DIR=$(realpath $INSTALL_DIR)
QT_BUILD_DIR=$(realpath $QT_BUILD_DIR)

# Configure Qt
pushd $QT_BUILD_DIR
$QT_DIR/configure.bat -release -static -static-runtime -optimize-size -prefix $INSTALL_DIR -platform win32-g++ -opensource -confirm-license	\
	-qt-zlib -qt-libpng -qt-webp -qt-libjpeg -qt-freetype -qt-tiff -qt-pcre																	\
	-no-jasper -no-opengl -no-mng -no-sql-odbc -make libs -nomake examples -nomake tests

# Build!
cmake --build . --parallel

# Install!
cmake --install .
popd
