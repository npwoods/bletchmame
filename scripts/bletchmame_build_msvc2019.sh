#!/bin/bash

###################################################################################
# bletchmame_build_msys2.sh - Builds BletchMAME under MSVC                        #
###################################################################################

# sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

# identify directories
BLETCHMAME_DIR=$(dirname $BASH_SOURCE)/..
BLETCHMAME_BUILD_DIR=build/msvc2019
BLETCHMAME_INSTALL_DIR=${BLETCHMAME_BUILD_DIR}
DEPS_INSTALL_DIR=$(realpath $(dirname $BASH_SOURCE)/../deps/msvc2019)

# toolset is v142 (MSVC2019), but can also be ClangCL
TOOLSET=v142

# config is Release by default, or 
CONFIG=Debug

# QT version is 6.1.2 by default
QT_VERSION=6.1.2

# parse arguments
while getopts "c:q:t:" OPTION; do
   case $OPTION in
      c)
         CONFIG=$OPTARG
		 ;;
      q)
         QT_VERSION=6.1.2
		 ;;
      t)
         TOOLSET=$OPTARG
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

if [ "$CONFIG" = Debug ]; then
  QUAZIP_LIBNAME=quazip1-qt6d.lib
else
  QUAZIP_LIBNAME=quazip1-qt6.lib
fi

# Set up build directory
rm -rf ${BLETCHMAME_BUILD_DIR}
cmake -S. -B${BLETCHMAME_BUILD_DIR}										\
	-G"Visual Studio 16 2019"											\
	-T"$TOOLSET"														\
	-DQt6_DIR=$QT6_INSTALL_DIR/lib/cmake/Qt6							\
	-DQt6Core_DIR=$QT6_INSTALL_DIR/lib/cmake/Qt6Core					\
	-DQt6CoreTools_DIR=$QT6_INSTALL_DIR/lib/cmake/Qt6CoreTools			\
	-DQt6GuiTools_DIR=$QT6_INSTALL_DIR/lib/cmake/Qt6GuiTools			\
	-DQt6Test_DIR=$QT6_INSTALL_DIR/lib/cmake/Qt6Test					\
	-DQt6Widgets_DIR=$QT6_INSTALL_DIR/lib/cmake/Qt6Widgets				\
	-DQt6WidgetsTools_DIR=$QT6_INSTALL_DIR/lib/cmake/Qt6WidgetsTools	\
	-DQuaZip-Qt6_DIR=$DEPS_INSTALL_DIR/lib/cmake/QuaZip-Qt6-1.1			\
	-DQUAZIP_LIBRARIES=$DEPS_INSTALL_DIR/lib/${QUAZIP_LIBNAME}			\
	-DCMAKE_LIBRARY_PATH=$DEPS_INSTALL_DIR/lib							\
	-DCMAKE_INCLUDE_PATH=$DEPS_INSTALL_DIR/include
cmake --build ${BLETCHMAME_BUILD_DIR} --parallel --config ${CONFIG}
cmake --install ${BLETCHMAME_INSTALL_DIR} --prefix ${BLETCHMAME_INSTALL_DIR} --config ${CONFIG}
