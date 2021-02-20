cmake_minimum_required(VERSION 3.15)

project(BletchMAME LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(CMAKE_FIND_LIBRARY_SUFFIXES .a)

# Threads
find_package( Threads )

# Expat
include(FindEXPAT)
find_package(EXPAT REQUIRED)
include_directories(${EXPAT_INCLUDE_DIRS})

# ZLib
include(FindZLIB)
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})

# Qt
find_package(Qt5 COMPONENTS Widgets LinguistTools REQUIRED)
find_package(Qt5Test REQUIRED)

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
target_link_libraries(BletchMAME_core PRIVATE Qt5::Widgets QuaZip)


#############################################################################
# BletchMAME executable                                                     #
#############################################################################

add_executable(BletchMAME WIN32 src/main.cpp src/resources.qrc)
target_include_directories(BletchMAME PRIVATE src lib)
target_link_libraries(BletchMAME PRIVATE BletchMAME_core Qt5::Widgets ${EXPAT_LIBRARIES} ${ZLIB_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})


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
target_link_libraries(BletchMAME_tests PRIVATE BletchMAME_core Qt5::Widgets Qt5::Test ${EXPAT_LIBRARIES} ${ZLIB_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})


#############################################################################
# QuaZib																	#
#############################################################################

add_library(QuaZip
	lib/quazip/ioapi.h
	lib/quazip/JlCompress.cpp
	lib/quazip/JlCompress.h
	lib/quazip/minizip_crypt.h
	lib/quazip/qioapi.cpp
	lib/quazip/quaadler32.cpp
	lib/quazip/quaadler32.h
	lib/quazip/quachecksum32.h
	lib/quazip/quacrc32.cpp
	lib/quazip/quacrc32.h
	lib/quazip/quagzipfile.cpp
	lib/quazip/quagzipfile.h
	lib/quazip/quaziodevice.cpp
	lib/quazip/quaziodevice.h
	lib/quazip/quazip.cpp
	lib/quazip/quazip.h
	lib/quazip/quazipdir.cpp
	lib/quazip/quazipdir.h
	lib/quazip/quazipfile.cpp
	lib/quazip/quazipfile.h
	lib/quazip/quazipfileinfo.cpp
	lib/quazip/quazipfileinfo.h
	lib/quazip/quazipnewinfo.cpp
	lib/quazip/quazipnewinfo.h
	lib/quazip/quazip_global.h
	lib/quazip/unzip.cpp
	lib/quazip/unzip.h
	lib/quazip/zip.cpp
	lib/quazip/zip.h
)
target_include_directories(QuaZip PRIVATE lib/quazip)
target_link_libraries(QuaZip PRIVATE Qt5::Widgets ${ZLIB_LIBRARIES})
add_definitions(-DQUAZIP_STATIC -DNOCRYPT)


#############################################################################
# Translations                                                              #
#############################################################################

qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
