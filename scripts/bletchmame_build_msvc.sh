#!/bin/bash

###################################################################################
# bletchmame_build_msvc.sh - Builds BletchMAME under MSVC                         #
###################################################################################

# sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

# toolset is v142 (MSVC2019), but can also be ClangCL
TOOLSET=v142

# config is "Debug" by default, but can also be "Release"
CONFIG=Debug
MSVC_VER=msvc2019

# QT version is 6.1.2 by default
QT_VERSION=6.1.2

# parse arguments
USE_PROFILER=off
while getopts "c:pq:t:v:" OPTION; do
   case $OPTION in
      c)
         CONFIG=$OPTARG
		 ;;
      p)
         USE_PROFILER=1
		 ;;
      q)
         QT_VERSION=6.1.2
		 ;;
      t)
         TOOLSET=$OPTARG
		 ;;
      v)
         MSVC_VER=$OPTARG
         ;;
   esac
done

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

# identify directories
BLETCHMAME_DIR=$(dirname $BASH_SOURCE)/..
BLETCHMAME_BUILD_DIR=build/$MSVC_VER
BLETCHMAME_INSTALL_DIR=${BLETCHMAME_BUILD_DIR}
DEPS_INSTALL_DIR=$(realpath $(dirname $BASH_SOURCE)/../deps/$MSVC_VER)

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

# set up build directory
rm -rf ${BLETCHMAME_BUILD_DIR}
cmake -S. -B${BLETCHMAME_BUILD_DIR}										\
	-G"$GENERATOR"	            										\
	-T"$TOOLSET"														\
	-DHAS_VERSION_GEN_H=1												\
	-DUSE_PROFILER=${USE_PROFILER}										\
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

# generate version.gen.h
mkdir -p ${BLETCHMAME_DIR}/include
git describe --tags | perl scripts/process_version.pl --versionhdr > ${BLETCHMAME_DIR}/include/version.gen.h

# and build!
cmake --build ${BLETCHMAME_BUILD_DIR} --parallel --config ${CONFIG}
cmake --install ${BLETCHMAME_INSTALL_DIR} --prefix ${BLETCHMAME_INSTALL_DIR} --config ${CONFIG}
