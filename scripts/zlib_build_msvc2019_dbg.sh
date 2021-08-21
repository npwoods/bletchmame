###################################################################################
# zlib_build_msvc2019.sh - Builds Zlib for MSVC (Debug)                        #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

# Identify directories
ZLIB_DIR=$(dirname $BASH_SOURCE)/../deps/zlib-ng
ZLIB_BUILD_DIR=$(dirname $BASH_SOURCE)/../deps/build/msvc2019_dbg/zlib-ng
INSTALL_DIR=$(realpath $(dirname $BASH_SOURCE)/../deps/msvc2019_dbg)

# Build and install it!
rm -rf $ZLIB_BUILD_DIR
cmake -S$ZLIB_DIR -B$ZLIB_BUILD_DIR		\
	--install-prefix $INSTALL_DIR		\
	-DCMAKE_BUILD_TYPE=Debug			\
	-G"Visual Studio 16 2019"
cmake --build $ZLIB_BUILD_DIR --parallel
cmake --install $ZLIB_BUILD_DIR --config Debug
