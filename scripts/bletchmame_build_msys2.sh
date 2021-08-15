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
QT_INSTALL_DIR=$(dirname $BASH_SOURCE)/../deps/qt6_msys2_install
QUAZIP_INSTALL_DIR=$(dirname $BASH_SOURCE)/../deps/quazip_msys2_install

cmake -S. -Bbuild_msys2 -DCMAKE_PREFIX_PATH=$QT_INSTALL_DIR/lib/cmake -DQuaZip-Qt6_DIR=$QUAZIP_INSTALL_DIR/lib/cmake/QuaZip-Qt6-1.1
