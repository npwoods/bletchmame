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

OBJ				= obj/x64_mingw
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

bin/x64_mingw/BletchMAME.exe:	$(OBJECTFILES) Makefile
	g++ -L$(WXWIDGETS_DIR)/lib/gcc_lib $(OBJECTFILES) $(LIBS) -o $@
	strip $@

obj/x64_mingw/%.o:	src/%.cpp Makefile
	g++ -I$(WXWIDGETS_DIR)/include -I$(WXWIDGETS_DIR)/lib/gcc_lib/mswu -O4 -c -o $@ $<
