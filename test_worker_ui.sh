#!/bin/sh
curl -L "https://github.com/mamedev/mame/releases/download/$1/$1b_64bit.exe" > deps/$1b_64bit.exe
7z -y x deps/$1b_64bit.exe -odeps/$1
./BletchMAME_tests.exe --runmame src/tests/scripts deps/$1/mame64.exe alienar -rompath deps/roms -skip_gameinfo -sound none -video none -pluginspath "deps/$1/plugins;plugins" -plugin worker_ui
