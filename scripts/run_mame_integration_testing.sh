#!/bin/bash

set -euo pipefail

###################################################################################
# run_mame_integration_testing.sh - Runs integration testing against actual MAME  #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

# Identify directories
SCRIPTS_DIR=$(dirname $BASH_SOURCE)
DEPS_DIR=$(dirname $BASH_SOURCE)/../deps

# Run it
mkdir -p $DEPS_DIR/roms
curl -L "https://www.mamedev.org/roms/alienar/alienar.zip" > $DEPS_DIR/roms/alienar.zip
cat <<EOF | parallel --joblog - $SCRIPTS_DIR/test_mame_interactions.sh
	mame0213
	mame0218
	mame0224
	mame0226
	mame0227
	mame0229
	mame0235
	mame0240
EOF
