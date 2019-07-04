#################################################################
# Crude BletchMAME Makefile for MinGW                           #
#                                                               #
# Only supporting MinGW x64 release builds for now              #
#                                                               #
# Prerequisite:  wxWidgets built for MinGW and dir specified in #
# WXWIDGETS_DIR                                                 #
#                                                               #
# See https://wiki.wxwidgets.org/Compiling_wxWidgets_with_MinGW #
#################################################################

BIN				= bin/mingw_win64/release
OBJ				= obj/mingw_win64/release
WXWIDGETS_LIBS	= -lwxmsw31u_core -lwxbase31u -lwxtiff -lwxjpeg -lwxpng -lwxzlib -lwxregexu -lwxexpat
LIBS			= $(WXWIDGETS_LIBS) -lkernel32 -luser32 -lgdi32 -lcomdlg32 -lwinspool -lwinmm -lshell32 -lshlwapi -lcomctl32 -lole32 -loleaut32 -luuid -lrpcrt4 -ladvapi32 -lversion -lwsock32 -lwininet -luxtheme -loleacc

OBJECTFILES=\
	$(OBJ)/app.o				\
	$(OBJ)/client.o				\
	$(OBJ)/dlgconsole.o			\
	$(OBJ)/dlgimages.o			\
	$(OBJ)/dlginputs.o			\
	$(OBJ)/dlgloading.o			\
	$(OBJ)/dlgpaths.o			\
	$(OBJ)/frame.o				\
	$(OBJ)/info.o				\
	$(OBJ)/job.o				\
	$(OBJ)/listxmltask.o		\
	$(OBJ)/prefs.o				\
	$(OBJ)/runmachinetask.o		\
	$(OBJ)/task.o				\
	$(OBJ)/utility.o			\
	$(OBJ)/validity.o			\
	$(OBJ)/virtuallistview.o	\
	$(OBJ)/xmlparser.o			\
	$(OBJ)/bletchmame.res.o		\

$(BIN)/BletchMAME.exe:	$(OBJECTFILES) Makefile $(BIN)/dir.txt
	g++ -static -static-libgcc -static-libstdc++ -L$(WXWIDGETS_DIR)/lib/gcc_lib $(OBJECTFILES) $(LIBS) -mwindows -o $@
	strip $@

$(OBJ)/%.o:	src/%.cpp Makefile $(OBJ)/dir.txt
	g++ -I$(WXWIDGETS_DIR)/include -I$(WXWIDGETS_DIR)/lib/gcc_lib/mswu -I./lib -O4 -std=c++17 -DWIN32 -c -o $@ $<

$(OBJ)/%.res.o:	src/%.rc Makefile $(OBJ)/dir.txt
	windres -I$(WXWIDGETS_DIR)/include -o $@ $<

%/dir.txt:
	sh -c "mkdir -p $(@D)"
	echo Directory Placeholder > $@
