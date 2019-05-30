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
WXWIDGETS_LIBS	= -lwxbase31u -lwxbase31u_net -lwxbase31u_xml -lwxexpat -lwxjpeg -lwxmsw31u_adv -lwxmsw31u_aui -lwxmsw31u_core -lwxmsw31u_gl -lwxmsw31u_html -lwxmsw31u_media -lwxmsw31u_propgrid -lwxmsw31u_ribbon -lwxmsw31u_richtext -lwxmsw31u_stc -lwxmsw31u_webview -lwxmsw31u_xrc -lwxpng -lwxregexu -lwxscintilla -lwxtiff -lwxzlib
LIBS			= -luser32 -lkernel32 -lgdi32 -lole32 -lmingw32 -lgcc -lmsvcrt -lstdc++ $(WXWIDGETS_LIBS)

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
	g++ -L$(WXWIDGETS_DIR)/lib/gcc_lib $(OBJECTFILES) $(LIBS) -o $@

obj/x64_mingw/%.o:	src/%.cpp
	g++ -I$(WXWIDGETS_DIR)/include -I$(WXWIDGETS_DIR)/lib/gcc_lib/mswu -O4 -c -o $@ $<
