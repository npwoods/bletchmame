#!/bin/bash

###################################################################################
# bletchmame_build_msys2.sh - Builds BletchMAME under MSYS2                       #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

# Identify directories
BLETCHMAME_DIR=$(dirname $BASH_SOURCE)/..
BLETCHMAME_BUILD_DIR=build/msys2
BLETCHMAME_INSTALL_DIR=${BLETCHMAME_BUILD_DIR}
DEPS_INSTALL_DIR=$(dirname $BASH_SOURCE)/../deps/install/msys2

# parse arguments
USE_PROFILER=off
BUILD_TYPE=Release					# can be Release/Debug/RelWithDebInfo etc
while getopts "pb:" OPTION; do
   case $OPTION in
      p)
         USE_PROFILER=1
		 ;;
      b)
         BUILD_TYPE=$OPTARG
         ;;
   esac
done

# set up build directory
rm -rf ${BLETCHMAME_BUILD_DIR}
echo "Build Type: $BUILD_TYPE"
cmake -S. -B${BLETCHMAME_BUILD_DIR}												\
	-DUSE_SHARED_LIBS=off														\
	-DHAS_VERSION_GEN_H=1														\
	-DUSE_PROFILER=${USE_PROFILER}												\
	-DCMAKE_BUILD_TYPE=${BUILD_TYPE}											\
	-DCMAKE_PREFIX_PATH="${DEPS_INSTALL_DIR}"									\
	-DQt6CoreTools_DIR=/mingw64/qt6-static/lib/cmake/Qt6CoreTools

# generate version.gen.h
mkdir -p ${BLETCHMAME_DIR}/include
git describe --tags | perl scripts/process_version.pl --versionhdr > ${BLETCHMAME_DIR}/include/version.gen.h

# and build!
cmake --build ${BLETCHMAME_BUILD_DIR} --parallel
cmake --install ${BLETCHMAME_BUILD_DIR} --strip --prefix ${BLETCHMAME_INSTALL_DIR}
