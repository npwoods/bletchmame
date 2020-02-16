# BletchMAME

BletchMAME is a new experimental front end for MAME.  Unlike existing front ends (which function as launchers, keeping MAME's internal UI), BletchMAME replaces the internal MAME UI with a more conventional point and click GUI to provide a friendlier experience in a number of areas (such as profiles, input configuration and a number of others).  While BletchMAME is intended to support all machines supported by MAME, it should be particularly suitable to computer emulation.

## Running BletchMAME

To run BletchMAME, run the installer (BletchMAME.msi on Windows) and BletchMAME will install, or install manually from the ZIP file.  The first thing you must do is point BletchMAME at a locally installed version of MAME 0.213 or later.  BletchMAME will take some time to read info from MAME, and when this is complete you can run the emulation.

## Version History

- 1.8 (2020-Feb-16)
	- Added command to drop into debugger
	- Fixed menu bar toggling bug

- 1.7 (2019-Dec-28)
	- We have a real domain name (https://www.bletchmame.org/); added a menu item to go directly there
	- If we fail to start MAME, we will now crudely present MAME's error message to the user
	- When supported by MAME (0.217 dirty or greater), we will attach to a child window rather than the BletchMAME root window.  This fixes a bug where the status bar could obscure part of the running emulation

- 1.6 (2019-Nov-24)
	- Upgraded to wxWidgets 3.1.3
	- Fixed a bug that could cause paths with parentheses to be incorrectly recognized (#1)

- 1.5 (2019-Nov-10)
	- Support for MAMEUI-style icon packs

- 1.4 (2019-Nov-3):
	- Full software list support
	- Fixed recently introduced bugs in the profiles view that could caused actions to be made against an incorrect profile

- 1.3 (2019-Oct-9):
	- Support for loading software list parts individually (but not yet full softlist software entries)
	- List views can now be sorted by column

- 1.2 (2019-Sep-18):
	- Improvements to the error reporting in the LUA plugin
	- Distributing BletchMAME as a ZIP file in addition to MSI
	- Fixed issues surrounding the "Mouse Inputs" button when changing inputs, including potential crashes
	- Fixed a bug that could cause the mouse to be captured at the start of an emulation session, even if the menu bar was showing

- 1.1 (2019-Sep-8):
	- Fixed bugs related to starting with a fresh configuration directory
	- Added basic "preflight checks" prior to starting MAME emulation sessions, to better catch obvious problems

- 1.0 (2019-Sep-7):
	- Initial Release

## Support for BletchMAME

Visit the BletchMAME website at http://bletchmame.s3-website-us-east-1.amazonaws.com/ or the IRC FreeNode channels #mame and #bletchmame
