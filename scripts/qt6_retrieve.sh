#!/bin/bash

###################################################################################
# qt6_retrieve.sh - Retrieves Qt sources                                          #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

# Initial retrieval of Qt
QT_DIR=$(dirname $BASH_SOURCE)/../deps/qt6
rm -rf $QT_DIR
mkdir -p $QT_DIR
git clone -b 6.1.2 --depth 1 https://code.qt.io/qt/qt5.git $QT_DIR

# Prep the repository
pushd $QT_DIR
perl init-repository --module-subset=essential,qtsvg,qtactiveqt,qtimageformats,qt5compat,qttools

# Apply hacks necessary to get static Qt to compile
patch -p1 <<EOF
diff --git a/qtbase/CMakeLists.txt b/qtbase/CMakeLists.txt
index f453505ee8..cee804ed20 100644
--- a/qtbase/CMakeLists.txt
+++ b/qtbase/CMakeLists.txt
@@ -176,3 +176,14 @@ qt_build_repo_end()
 if(NOT QT_BUILD_STANDALONE_TESTS AND QT_BUILD_EXAMPLES)
     add_subdirectory(examples)
 endif()
+
+###################################################################################
+# HACKS TO GET STATIC QT TO COMPILE FOR BLETCHMAME                                #
+###################################################################################
+
+if(TARGET zstd::libzstd_static)
+    set_property(TARGET zstd::libzstd_shared PROPERTY IMPORTED_GLOBAL TRUE)
+endif()
+if(TARGET zstd::libzstd_shared)
+    set_property(TARGET zstd::libzstd_static PROPERTY IMPORTED_GLOBAL TRUE)
+endif()
EOF

# We're done!
popd
