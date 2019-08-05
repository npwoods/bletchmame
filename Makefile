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

ifdef DEBUG
BUILD			= debug
CFLAGS			= -O0 -g
else
BUILD			= release
CFLAGS			= -O4
endif

BIN				= bin/mingw_win64/$(BUILD)
OBJ				= obj/mingw_win64/$(BUILD)
WXWIDGETS_LIBS	= -lwxmsw31u_core -lwxbase31u -lwxtiff -lwxjpeg -lwxpng -lwxzlib -lwxregexu -lwxexpat
LIBS			= $(WXWIDGETS_LIBS) -lkernel32 -luser32 -lgdi32 -lcomdlg32 -lwinspool -lwinmm -lshell32 -lshlwapi -lcomctl32 -lole32 -loleaut32 -luuid -lrpcrt4 -ladvapi32 -lversion -lwsock32 -lwininet -luxtheme -loleacc
CFLAGS			+= -I$(WXWIDGETS_DIR)/include -I$(WXWIDGETS_DIR)/lib/gcc_lib/mswu -I$(WXWIDGETS_DIR)/src/expat/expat/lib -I./lib -I./src -std=c++17 -DWIN32 
MKDIR_RULE		= @sh -c "mkdir -p $(@D)"

ifndef WXWIDGETS_DIR
WXWIDGETS_DIR=lib/wxWidgets
endif

OBJECTFILES=\
	$(OBJ)/app.o				\
	$(OBJ)/client.o				\
	$(OBJ)/dialogs/console.o	\
	$(OBJ)/dialogs/images.o		\
	$(OBJ)/dialogs/inputs.o		\
	$(OBJ)/dialogs/loading.o	\
	$(OBJ)/dialogs/paths.o		\
	$(OBJ)/dialogs/switches.o	\
	$(OBJ)/frame.o				\
	$(OBJ)/info.o				\
	$(OBJ)/job.o				\
	$(OBJ)/listxmltask.o		\
	$(OBJ)/prefs.o				\
	$(OBJ)/runmachinetask.o		\
	$(OBJ)/status.o				\
	$(OBJ)/task.o				\
	$(OBJ)/utility.o			\
	$(OBJ)/validity.o			\
	$(OBJ)/versiontask.o		\
	$(OBJ)/virtuallistview.o	\
	$(OBJ)/xmlparser.o			\
	$(OBJ)/bletchmame.res.o		\

$(BIN)/BletchMAME.exe:	$(OBJECTFILES) Makefile
	$(MKDIR_RULE)
	g++ -static -static-libgcc -static-libstdc++ -L$(WXWIDGETS_DIR)/lib/gcc_lib $(OBJECTFILES) $(LIBS) -mwindows -o $@
ifndef DEBUG
	strip $@
endif

$(OBJ)/%.o:	src/%.cpp Makefile
	$(MKDIR_RULE)
	g++ $(CFLAGS) -c -o $@ $<

$(OBJ)/dialogs/%.o:	src/dialogs/%.cpp Makefile
	sh -c "mkdir -p $(@D)"
	g++ $(CFLAGS) -c -o $@ $<

$(OBJ)/%.s:	src/%.cpp Makefile
	$(MKDIR_RULE)
	g++ $(CFLAGS) -S -o $@ $<

$(OBJ)/dialogs/%.s:	src/dialogs/%.cpp Makefile
	$(MKDIR_RULE)
	g++ $(CFLAGS) -S -o $@ $<

$(OBJ)/%.res.o:	src/%.rc Makefile
	$(MKDIR_RULE)
	windres -I$(WXWIDGETS_DIR)/include -o $@ $<

clean:
	rm -rf obj bin
