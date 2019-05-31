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
WXWIDGETS_LIBS	= -lwxmsw31u_core -lwxbase31u -lwxbase31u_xml -lwxtiff -lwxjpeg -lwxpng -lwxzlib -lwxregexu -lwxexpat
LIBS			= $(WXWIDGETS_LIBS) -lkernel32 -luser32 -lgdi32 -lcomdlg32 -lwinspool -lwinmm -lshell32 -lshlwapi -lcomctl32 -lole32 -loleaut32 -luuid -lrpcrt4 -ladvapi32 -lversion -lwsock32 -lwininet -luxtheme -loleacc

OBJECTFILES=\
	$(OBJ)/app.o				\
	$(OBJ)/client.o				\
	$(OBJ)/dlgpaths.o			\
	$(OBJ)/frame.o				\
	$(OBJ)/job.o				\
	$(OBJ)/listxmltask.o		\
	$(OBJ)/prefs.o				\
	$(OBJ)/runmachinetask.o		\
	$(OBJ)/utility.o			\
	$(OBJ)/validity.o			\
	$(OBJ)/virtuallistview.o	\

$(BIN)/BletchMAME.exe:	$(OBJECTFILES) Makefile $(BIN)
	g++ -L$(WXWIDGETS_DIR)/lib/gcc_lib $(OBJECTFILES) $(LIBS) -o $@
	strip $@

$(OBJ)/%.o:	src/%.cpp Makefile $(OBJ)
	g++ -I$(WXWIDGETS_DIR)/include -I$(WXWIDGETS_DIR)/lib/gcc_lib/mswu -O4 -c -o $@ $<

$(BIN):
	mkdir bin\\mingw_win64\\release

$(OBJ):
	mkdir obj\\mingw_win64\\release

