# BletchMAME

BletchMAME is a new experimental front end for MAME.  Unlike existing front ends (which function as launchers, keeping MAME's internal UI), BletchMAME replaces the internal MAME UI with a more conventional point and click GUI to provide a friendlier experience in a number of areas (such as profiles, input configuration and a number of others).  While BletchMAME is intended to support all machines supported by MAME, it should be particularly suitable to computer emulation.

## Running BletchMAME

To run BletchMAME, run the installer (BletchMAME.msi on Windows) and BletchMAME will install, or install manually from the ZIP file.  The first thing you must do is point BletchMAME at a locally installed version of MAME 0.213 or later.  BletchMAME will take some time to read info from MAME, and when this is complete you can run the emulation.

## Version History

- 2.11 (TBD)
	- Enabled Link Time Optimization for Release builds
	- CI builds against Qt6.2 [Davide Cavalca]

- 2.10 (2021-Dec-4)
	- Added an "Available" folder to the machine tree view (like MAMEUI)
	- Added the ability to reset settings to default
	- Current selections and auditing results will be described in the status bar
	- Various improvements to auditing
		- Media auditing will now proceed even when visible items have been audited
		- Changing ROMs and Samples paths will only force reaudits of items that use the relevant media
		- Fixed some bugs that could cause audit icons to not be properly painted
	- Hash paths will now default to 'hash' in the MAME directory
	- Fixed a bug that could cause switching between paths in the paths dialog to not persist changes
	- Various improvements to the CI/CD pipeline [Davide Cavalca]

- 2.9 (2021-Oct-23)
	- Added experimental MAMEUI-style media auditing capabilities
		- ROMs, Samples, Disks and Software Lists are supported
	- When refreshing the MAME Info Database (-listxml handling), names and descriptions of machines will be displayed
	- No longer waiting for the MAME version check to complete on startup
	- Various enhancements to the Paths Dialog
		- Highlighting file paths for snapshots and icons that are not valid archives in red
		- Multi selection of paths is now supported
		- Minor bug fixes to the logic that updates the status of the Browse/Insert/Delete buttons
	- Improving display of icon on the machine list on high resolution displays
	- Now remembering the maximized/full screen and size state of the main window
	- Fixed a bug that caused column sorts to not be honored on startup
	- Fixed a bug that caused newly created profiles to not get selected when created
	- Fixed a bug that caused the software list tab to not be populated is selected on startup
	- Small optimizations to the building of MAME Info Database and general XML parsing

- 2.8 (2021-Aug-29)
	- Added support for snapshots in ZIP files instead of in directories (#128)
	- Fixed an issue in Info XML parsing caused by inappropriately relying on the current locale's rules (#143) [Julian Sikorski]
		- This rendered BletchMAME unusable in some locales
	- Fixed a bug where updates to the snapshot path failed to trigger an update of the info panel
	- Removed accidentally introduced dependency on libwinpthread-1.dll

- 2.7 (2021-Aug-27)
	- Updated to Qt 6.1
		- On Windows, Qt is now built statically and optimized for size
	- Fixed a minor bug that caused garbage icons to be displayed for systems without icons
	- Supporting -attach_window on SDLMAME 0.232 and higher
	- Omitting -attach_window when MAME identified as lacking support
	- Various changes for running under Linux [Davide Cavalca]
		- CMake and build tweaks to link with dependencies non-statically
		- Created basic desktop and metadata files for GNOME
	- Small optimizations to the building of MAME Info Database (-listxml handling)

- 2.6 (2021-Mar-6)
	- Added a number of MAMEUI-style builtin folders 
		- BIOS, CHD, CPU, Dumping, Mechanical, Non Mechanical, Raster, Samples, Save State, Sound, Unofficial, Vector
	- Added support for custom folders
	- Added a "Show Folders" contextual menu to the folder tree
	- Fixed a unicode issue in preferences handling
	- Updated to C++20

- 2.5 (2021-Jan-31)
	- Beginings of a MAMEUI-style machine folder and snapshot view

- 2.4 (2021-Jan-3)
	- Updated various parts of the worker_ui plugin in response to breaking LUA changes in MAME 0.227
		- An implication is that previous versions of BletchMAME will not work with MAME 0.227 or greater
		- Because MAME 0.227 doesn't support hot changes to mouse capture, the mouse may not be usable
			- BletchMAME will warn the user in this scenario
	- Changed the Images Dialog to a Configurable Devices dialog, combining image and slot selection
		- Slot modification requires MAME 0.227, and will not function in previouew versions
	- Resurrected the "Quick Load/Save State" feature, which has been broken since 2.0
	- Fixed profile renaming, which also has been broken since 2.0
	- Fixed a bug in the profiles view that could cause a crash if one right clicks empty space

- 2.3 (2020-Dec-13)
	- Added a warning to the Images Dialog if no hash paths were set up
	- Fixed a bug that could cause image widgets to not be properly cleaned up if they got removed by a slot change
	- Fixed a bug that prevented the "Unload Image" menu item from working
	- Fixed a bug in the console window that could cause text to be appended in the wrong location
	- Fixed a cosmetic bug in the "About BletchMAME" dialog
	- No longer passing -nodtd when invoking -listxml on MAME
	- Changed worker_ui plugin to tolerate changes in machine().images expected with MAME 0.227

- 2.2 (2020-Oct-3)
	- Support for cheats (requires MAME 0.225)
	- The worker_ui plugin is now tested automatically as a part of the CI/CD pipeline

- 2.1 (2020-Aug-29)
	- Better handling of scenarios when the preferences directory does not exist
	- Various changes to build on Linux [Davide Cavalca]

- 2.0 (2020-Aug-23)
	- Overhaul of BletchMAME application
	- Transitioned from wxWidgets to Qt
	- Adopted CMake
	- Unit tests now executed as part of build pipeline

- 1.9 (2020-May-20)
	- Added the ability to record AVI/MNG movies (requires MAME 0.221)
	- Fixed a bug that would choose odd default file name when saving screenshots
	- Ensuring that directory paths always have trailing path separators

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

Visit the BletchMAME website at https://www.bletchmame.org/ or the IRC FreeNode channels #mame and #bletchmame
