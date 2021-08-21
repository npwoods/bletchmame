###################################################################################
# expat_build_msvc2019.sh - Builds QuaZip for MSVC (Debug)                        #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

# Identify directories
EXPAT_DIR=$(dirname $BASH_SOURCE)/../deps/expat/expat
EXPAT_BUILD_DIR=$(dirname $BASH_SOURCE)/../deps/build/msvc2019_dbg/expat
INSTALL_DIR=$(dirname $BASH_SOURCE)/../deps/msvc2019_dbg

# Build and install it!
rm -rf $EXPAT_BUILD_DIR
cmake -S$EXPAT_DIR -B$EXPAT_BUILD_DIR								\
	-DCMAKE_BUILD_TYPE=Debug										\
	-G"Visual Studio 16 2019"
cmake --build $EXPAT_BUILD_DIR --parallel
cmake --install $EXPAT_BUILD_DIR --prefix $INSTALL_DIR --config Debug
