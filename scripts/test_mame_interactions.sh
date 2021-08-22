#!/bin/bash

#############################################################################################
# test_mame_interactions.sh - Use BletchMAME_tests to exercise a particular version of MAME #
#############################################################################################

# Determine the MAME version (e.g. - 'mame0227')
MAME_VERSION="$(echo -e "${1}" | tr -d '[:space:]')"
MAME_VERSION_NUMBER="$(echo -e "${MAME_VERSION}" | tr -d '[a-z]')"

# Determine the MAME executable name
if [ $MAME_VERSION_NUMBER -le 228 ]; then
    MAME_EXE=mame64.exe
else
    MAME_EXE=mame.exe
fi

# Abort if anything fails
set -e

# Download MAME binaries for Windows
curl -f -L "https://github.com/mamedev/mame/releases/download/${MAME_VERSION}/${MAME_VERSION}b_64bit.exe" > deps/${MAME_VERSION}b_64bit.exe

# Extract the archive
7z -y x deps/${MAME_VERSION}b_64bit.exe -odeps/${MAME_VERSION}

# Run integration tests that put MAME through its courses
./build/msys2/BletchMAME_tests.exe --runmame src/tests/scripts deps/${MAME_VERSION}/${MAME_EXE} alienar -rompath deps/roms -skip_gameinfo -sound none -video none -nothrottle -pluginspath "deps/${MAME_VERSION}/plugins;plugins" -plugin worker_ui

# Ensure we can digest -listxml from this MAME
./build/msys2/BletchMAME_tests.exe --runlistxml deps/${MAME_VERSION}/${MAME_EXE}

# Finally clean up after ourselves
rm -rf deps/${MAME_VERSION}b_64bit.exe deps/${MAME_VERSION}/
