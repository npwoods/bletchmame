#!/bin/sh

# Determine the MAME version (e.g. - 'mame0227')
MAME_VERSION="$(echo -e "${1}" | tr -d '[:space:]')"

# Abort if anything fails
set -e

# Download MAME binaries for Windows
curl -f -L "https://github.com/mamedev/mame/releases/download/${MAME_VERSION}/${MAME_VERSION}b_64bit.exe" > deps/${MAME_VERSION}b_64bit.exe

# Extract the archive
7z -y x deps/${MAME_VERSION}b_64bit.exe -odeps/${MAME_VERSION}

# Run integration tests that put MAME through its courses
./BletchMAME_tests.exe --runmame src/tests/scripts deps/${MAME_VERSION}/mame64.exe alienar -rompath deps/roms -skip_gameinfo -sound none -video none -nothrottle -pluginspath "deps/${MAME_VERSION}/plugins;plugins" -plugin worker_ui

# Ensure we can digest -listxml from this MAME
./BletchMAME_tests.exe --runlistxml deps/${MAME_VERSION}/mame64.exe

# Finally clean up after ourselves
rm -rf deps/${MAME_VERSION}b_64bit.exe deps/${MAME_VERSION}/
