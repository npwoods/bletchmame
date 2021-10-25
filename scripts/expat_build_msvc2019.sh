#!/bin/bash

set -euo pipefail

###################################################################################
# expat_build_msvc2019.sh - Builds Expat for MSVC                                 #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

CONFIG=Debug

# parse arguments
while getopts "c:" OPTION; do
   case $OPTION in
      c)
         CONFIG=$OPTARG
         ;;
   esac
done

# Identify directories
EXPAT_DIR=$(dirname $BASH_SOURCE)/../deps/expat/expat
EXPAT_BUILD_DIR=$(dirname $BASH_SOURCE)/../deps/build/msvc2019/expat
INSTALL_DIR=$(dirname $BASH_SOURCE)/../deps/msvc2019

# Build and install it!
rm -rf $EXPAT_BUILD_DIR
cmake -S$EXPAT_DIR -B$EXPAT_BUILD_DIR -G"Visual Studio 16 2019"
cmake --build $EXPAT_BUILD_DIR --parallel --config $CONFIG
cmake --install $EXPAT_BUILD_DIR --prefix $INSTALL_DIR --config $CONFIG
