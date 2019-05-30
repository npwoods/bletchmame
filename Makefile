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

OBJ		= obj/x64_mingw
LIBS	= -luser32 -lkernel32 -lmingw32 -lgcc -lmsvcrt -lwxbase31u -lwxbase31u_net -lwxbase31u_xml -lwxmsw31u_core -lstdc++

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

bin/x64_mingw/BletchMAME.exe:	$(OBJECTFILES)
	gcc -L$(WXWIDGETS_DIR)/lib/gcc_lib $(LIBS) $(OBJECTFILES) -o $@

obj/x64_mingw/%.o:	src/%.cpp
	gcc -I$(WXWIDGETS_DIR)/include -I$(WXWIDGETS_DIR)/lib/gcc_lib/mswu -O4 -c -o $@ $<
