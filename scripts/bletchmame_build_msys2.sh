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
DEPS_INSTALL_DIR=$(dirname $BASH_SOURCE)/../deps/msys2

# Generate buildversion.txt
git describe --tags | python version.py | cat >buildversion.txt
echo "Build Version: $(cat buildversion.txt)"

# Generate src/buildversion.gen.cpp
echo >$BLETCHMAME_DIR/src/buildversion.gen.cpp  "const char buildVersion[] = \"`cat buildversion.txt 2>NUL`\";"
echo >>$BLETCHMAME_DIR/src/buildversion.gen.cpp "const char buildRevision[] = \"`git rev-parse HEAD 2>NUL`\";"
echo >>$BLETCHMAME_DIR/src/buildversion.gen.cpp "const char buildDateTime[] = \"`date -Ins`\";"

# parse arguments
USE_PROFILER=off
BUILD_TYPE=Release
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

# Set up build directory
rm -rf ${BLETCHMAME_BUILD_DIR}
echo "Build Type: $BUILD_TYPE"
cmake -S. -B${BLETCHMAME_BUILD_DIR}												\
	-DUSE_SHARED_LIBS=off -DHAS_BUILDVERSION_GEN_CPP=1 							\
	-DUSE_PROFILER=${USE_PROFILER}												\
	-DCMAKE_BUILD_TYPE=${BUILD_TYPE}											\
	-DQt6_DIR=${DEPS_INSTALL_DIR}/lib/cmake/Qt6									\
	-DQt6BundledFreetype_DIR=${DEPS_INSTALL_DIR}/lib/cmake/Qt6BundledFreetype	\
	-DQt6BundledHarfbuzz_DIR=${DEPS_INSTALL_DIR}/lib/cmake/Qt6BundledHarfbuzz	\
	-DQt6BundledLibpng_DIR=${DEPS_INSTALL_DIR}/lib/cmake/Qt6BundledLibpng		\
	-DQt6BundledPcre2_DIR=${DEPS_INSTALL_DIR}/lib/cmake/Qt6BundledPcre2			\
	-DQt6Core_DIR=${DEPS_INSTALL_DIR}/lib/cmake/Qt6Core							\
	-DQt6CoreTools_DIR=${DEPS_INSTALL_DIR}/lib/cmake/Qt6CoreTools				\
	-DQt6GuiTools_DIR=${DEPS_INSTALL_DIR}/lib/cmake/Qt6GuiTools					\
	-DQt6Test_DIR=${DEPS_INSTALL_DIR}/lib/cmake/Qt6Test							\
	-DQt6WidgetsTools_DIR=${DEPS_INSTALL_DIR}/lib/cmake/Qt6WidgetsTools			\
	-DQuaZip-Qt6_DIR=${DEPS_INSTALL_DIR}/lib/cmake/QuaZip-Qt6-1.1

# And build!
cmake --build ${BLETCHMAME_BUILD_DIR} --parallel
cmake --install ${BLETCHMAME_BUILD_DIR} --strip --prefix ${BLETCHMAME_INSTALL_DIR}
