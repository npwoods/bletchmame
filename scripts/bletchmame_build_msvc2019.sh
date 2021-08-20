#!/bin/bash

###################################################################################
# bletchmame_build_msys2.sh - Builds BletchMAME under MSVC (Debug)                #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi
if [ -z "$QT_INSTALL_DIR" ]; then
  echo "Null QT_INSTALL_DIR"
  exit
fi
if [ -z "$EXPAT_LIBRARY" ]; then
  echo "Null EXPAT_LIBRARY"
  exit
fi
if [ -z "$EXPAT_INCLUDE_DIR" ]; then
  echo "Null EXPAT_INCLUDE_DIR"
  exit
fi

# Identify directories
BLETCHMAME_DIR=$(dirname $BASH_SOURCE)/..
BLETCHMAME_BUILD_DIR=build/msvc
BLETCHMAME_INSTALL_DIR=${BLETCHMAME_BUILD_DIR}
QUAZIP_INSTALL_DIR=$(dirname $BASH_SOURCE)/../deps/quazip_msvc_install

# Set up build directory
rm -rf ${BLETCHMAME_BUILD_DIR}
cmake -S. -B${BLETCHMAME_BUILD_DIR}									\
	-G"Visual Studio 16 2019"										\
	-DQt6_DIR=$QT_INSTALL_DIR/lib/cmake/Qt6							\
	-DQt6Core_DIR=$QT_INSTALL_DIR/lib/cmake/Qt6Core					\
	-DQt6CoreTools_DIR=$QT_INSTALL_DIR/lib/cmake/Qt6CoreTools		\
	-DQt6GuiTools_DIR=$QT_INSTALL_DIR/lib/cmake/Qt6GuiTools			\
	-DQt6Test_DIR=$QT_INSTALL_DIR/lib/cmake/Qt6Test					\
	-DQt6Widgets_DIR=$QT_INSTALL_DIR/lib/cmake/Qt6Widgets			\
	-DQt6WidgetsTools_DIR=$QT_INSTALL_DIR/lib/cmake/Qt6WidgetsTools	\
	-DQt6Zlib_DIR=$QT_INSTALL_DIR/lib/cmake/Qt6Zlib					\
	-DQuaZip-Qt6_DIR=$QUAZIP_INSTALL_DIR/lib/cmake/QuaZip-Qt6-1.1	\
	-DEXPAT_LIBRARY=$EXPAT_LIBRARY									\
	-DEXPAT_INCLUDE_DIR=$EXPAT_INCLUDE_DIR							\
	-DZLIB_LIBRARY=$ZLIB_LIBRARY									\
	-DZLIB_INCLUDE_DIR=$ZLIB_INCLUDE_DIR
cmake --build ${BLETCHMAME_BUILD_DIR} --parallel
cmake --install ${BLETCHMAME_INSTALL_DIR} --prefix ${BLETCHMAME_INSTALL_DIR}