#!/bin/bash

set -euo pipefail

###################################################################################
# expat_retrieve.sh - Retrieves Expat sources                                     #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

# Retrieval of Expat
EXPAT_DIR=$(dirname $BASH_SOURCE)/../deps/expat
rm -rf $EXPAT_DIR
git clone -b R_2_4_1 --depth 1 https://github.com/libexpat/libexpat.git $EXPAT_DIR
