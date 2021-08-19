###################################################################################
# quazip_build_msvc.sh - Builds QuaZip for MSVC (Debug)                           #
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
if [ -z "$ZLIB_LIBRARY" ]; then
  echo "Null ZLIB_LIBRARY"
  exit
fi
if [ -z "$ZLIB_INCLUDE_DIR" ]; then
  echo "Null ZLIB_INCLUDE_DIR"
  exit
fi

# Identify directories
QUAZIP_DIR=$(dirname $BASH_SOURCE)/../deps/quazip
QUAZIP_BUILD_DIR=$(dirname $BASH_SOURCE)/../deps/quazip_msvc_build
QUAZIP_INSTALL_DIR=$(dirname $BASH_SOURCE)/../deps/quazip_msvc_install

# Build and install it!
rm -rf $QUAZIP_BUILD_DIR $QUAZIP_INSTALL_DIR
cmake -S$QUAZIP_DIR -B$QUAZIP_BUILD_DIR -DQUAZIP_QT_MAJOR_VERSION=6	\
	-DCMAKE_BUILD_TYPE=Debug										\
	-G"Visual Studio 16 2019"										\
	-DQt6_DIR=$QT_INSTALL_DIR/lib/cmake/Qt6							\
	-DQt6Core_DIR=$QT_INSTALL_DIR/lib/cmake/Qt6Core					\
	-DQt6CoreTools_DIR=$QT_INSTALL_DIR/lib/cmake/Qt6CoreTools		\
	-DZLIB_LIBRARY=$ZLIB_LIBRARY									\
	-DZLIB_INCLUDE_DIR=$ZLIB_INCLUDE_DIR
cmake --build $QUAZIP_BUILD_DIR --parallel
cmake --install $QUAZIP_BUILD_DIR --prefix $QUAZIP_INSTALL_DIR --config Debug
