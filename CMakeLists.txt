cmake_minimum_required(VERSION 3.15)

project(BletchMAME LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# By default, BletchMAME should build linking to shared libraries; its the norm on Linux and it
# is desirable in debugging scenarios.  But for when performing releases on platforms where such
# releases may entail distributing large Qt libraries (Windows and probably macOS), building linking
# to Qt and QuaZip statically is desirable to minimize dependency issues
option(USE_SHARED_LIBS "Use shared libraries" ON)
if (NOT USE_SHARED_LIBS)
set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
set(CMAKE_CXX_STANDARD_LIBRARIES "-static-libgcc -static-libstdc++ ${CMAKE_CXX_STANDARD_LIBRARIES}")
add_definitions(-DQUAZIP_STATIC)
endif()


#############################################################################
# Dependencies                                                              #
#############################################################################

# Threads
find_package( Threads )

# Expat
include(FindEXPAT)
find_package(EXPAT REQUIRED)
include_directories(${EXPAT_INCLUDE_DIR})

# ZLib
include(FindZLIB)
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})

# Qt
find_package(Qt6 COMPONENTS Widgets LinguistTools Core5Compat REQUIRED)
find_package(Qt6Test REQUIRED)
find_package(Qt6Zlib REQUIRED)

# QuaZip
find_package(QuaZip-Qt6 REQUIRED)
include_directories(SYSTEM ${QuaZip-Qt6_DIR}/../../../include/QuaZip-Qt6-1.1/quazip)
link_directories(${QuaZip-Qt6_DIR}/../../../lib)

# Yes, we're unit testing
enable_testing(true)

# Is version.gen.cpp available?
if (HAS_BUILDVERSION_GEN_CPP)
MESSAGE(STATUS "buildversion.gen.cpp AVAILABLE")
add_definitions(-DHAS_BUILDVERSION_GEN_CPP=1)
else()
MESSAGE(STATUS "buildversion.gen.cpp UNAVAILABLE")
add_definitions(-DHAS_BUILDVERSION_GEN_CPP=0)
endif()

# Define installation directories
include(GNUInstallDirs)


#############################################################################
# BletchMAME core (library shared by executable and tests)                  #
#############################################################################

add_library(BletchMAME_core
	src/buildversion.cpp
	src/buildversion.h
	src/client.cpp
	src/client.h
	src/iconloader.cpp
	src/iconloader.h
	src/info.cpp
	src/info.h
	src/info_builder.cpp
	src/info_builder.h
	src/job.cpp
	src/job.h
	src/listxmltask.cpp
	src/listxmltask.h
	src/machinefoldertreemodel.cpp
	src/machinefoldertreemodel.h
	src/machinelistitemmodel.cpp
	src/machinelistitemmodel.h
	src/mainpanel.cpp
	src/mainpanel.h
	src/mainpanel.ui
	src/mainwindow.cpp
	src/mainwindow.h
	src/mainwindow.ui
	src/mameworkercontroller.cpp
	src/mameworkercontroller.h
	src/mameversion.cpp
	src/mameversion.h
	src/messagequeue.h
	src/prefs.cpp
	src/prefs.h
	src/profile.cpp
	src/profile.h
	src/profilelistitemmodel.cpp
	src/profilelistitemmodel.h
	src/runmachinetask.cpp
	src/runmachinetask.h
	src/sessionbehavior.cpp
	src/sessionbehavior.h
	src/softwarelist.cpp
	src/softwarelist.h
	src/softwarelistitemmodel.cpp
	src/softwarelistitemmodel.h
	src/splitterviewtoggler.cpp
	src/splitterviewtoggler.h
	src/status.cpp
	src/status.h
	src/utility.cpp
	src/utility.h
	src/tableviewmanager.cpp
	src/tableviewmanager.h
	src/task.cpp
	src/task.h	
	src/versiontask.cpp
	src/versiontask.h
	src/xmlparser.cpp
	src/xmlparser.h
	src/dialogs/about.cpp
	src/dialogs/about.h
	src/dialogs/about.ui
	src/dialogs/cheats.cpp
	src/dialogs/cheats.h
	src/dialogs/cheats.ui
	src/dialogs/choosesw.cpp
	src/dialogs/choosesw.h
	src/dialogs/choosesw.ui
	src/dialogs/confdev.cpp
	src/dialogs/confdev.h
	src/dialogs/confdev.ui
	src/dialogs/confdevmodel.cpp
	src/dialogs/confdevmodel.h
	src/dialogs/console.cpp
	src/dialogs/console.h
	src/dialogs/console.ui
	src/dialogs/inputs.cpp
	src/dialogs/inputs.h
	src/dialogs/inputs_base.cpp
	src/dialogs/inputs_base.h
	src/dialogs/inputs_base.ui
	src/dialogs/inputs_multiaxis.cpp
	src/dialogs/inputs_multiaxis.h
	src/dialogs/inputs_multiaxis.ui
	src/dialogs/inputs_multiquick.cpp
	src/dialogs/inputs_multiquick.h
	src/dialogs/inputs_multiquick.ui
	src/dialogs/inputs_seqpoll.cpp
	src/dialogs/inputs_seqpoll.h
	src/dialogs/inputs_seqpoll.ui
	src/dialogs/loading.cpp
	src/dialogs/loading.h
	src/dialogs/loading.ui
	src/dialogs/newcustomfolder.cpp
	src/dialogs/newcustomfolder.h
	src/dialogs/newcustomfolder.ui
	src/dialogs/paths.cpp
	src/dialogs/paths.h
	src/dialogs/paths.ui
	src/dialogs/switches.cpp
	src/dialogs/switches.h
	${TS_FILES}
)
target_include_directories(BletchMAME_core PRIVATE src lib)
target_link_libraries(BletchMAME_core PRIVATE Qt6::Widgets quazip1-qt6 Qt6::Core5Compat ${ZLIB_LIBRARIES})


#############################################################################
# BletchMAME executable                                                     #
#############################################################################

add_executable(BletchMAME WIN32 src/main.cpp src/resources.qrc)
target_include_directories(BletchMAME PRIVATE src lib)
target_link_libraries(BletchMAME PRIVATE BletchMAME_core Qt6::Widgets quazip1-qt6 Qt6::Core5Compat ${EXPAT_LIBRARIES} ${ZLIB_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} -static gcc stdc++ winpthread)
install(TARGETS BletchMAME RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})


#############################################################################
# BletchMAME unit tests                                                     #
#############################################################################

add_executable(BletchMAME_tests	
	src/tests/test.cpp
	src/tests/test.h
	src/tests/listxmlrunner.cpp
	src/tests/mamerunner.cpp
	src/tests/client_test.cpp
	src/tests/info_test.cpp
	src/tests/info_builder_test.cpp
	src/tests/listxmltask_test.cpp
	src/tests/machinefoldertreemodel_test.cpp
	src/tests/machinelistitemmodel_test.cpp
	src/tests/mameversion_test.cpp
	src/tests/prefs_test.cpp
	src/tests/profile_test.cpp
	src/tests/runmachinetask_test.cpp
	src/tests/softwarelist_test.cpp
	src/tests/status_test.cpp
	src/tests/utility_test.cpp
	src/tests/xmlparser_test.cpp
	src/tests/dialogs/about_test.cpp
	src/tests/dialogs/confdevmodel_test.cpp
	src/tests/dialogs/inputs_test.cpp
	src/tests/dialogs/paths_test.cpp
	src/tests/resources.qrc
	src/resources.qrc
)
add_test(NAME BletchMAME_tests COMMAND BletchMAME_tests)
target_include_directories(BletchMAME_tests PRIVATE src lib src/test)
target_link_libraries(BletchMAME_tests PRIVATE BletchMAME_core Qt6::Widgets Qt6::Test quazip1-qt6 Qt6::Core5Compat ${EXPAT_LIBRARIES} ${ZLIB_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})


#############################################################################
# Translations                                                              #
#############################################################################

qt6_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
