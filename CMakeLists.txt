cmake_minimum_required(VERSION 3.5)

project(BletchMAME LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FindEXPAT)
find_package(EXPAT REQUIRED)
include_directories(${EXPAT_INCLUDE_DIRS})

enable_testing(true)

find_package(Qt5 COMPONENTS Widgets LinguistTools REQUIRED)
find_package(Qt5Test REQUIRED)


#############################################################################
# BletchMAME core (library shared by executable and tests)                  #
#############################################################################

add_library(BletchMAME_core
	src/client.cpp
	src/client.h
	src/info.cpp
	src/info.h
	src/info_builder.cpp
	src/info_builder.h
	src/job.cpp
	src/job.h
	src/listxmltask.cpp
	src/listxmltask.h
	src/machinelistitemmodel.cpp
	src/machinelistitemmodel.h
	src/mainwindow.cpp
	src/mainwindow.h
	src/mainwindow.ui
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
	src/softwarelist.cpp
	src/softwarelist.h
	src/softwarelistitemmodel.cpp
	src/softwarelistitemmodel.h
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
	src/dialogs/loading.cpp
	src/dialogs/loading.h
	src/dialogs/loading.ui
	src/dialogs/paths.cpp
	src/dialogs/paths.h
	src/dialogs/paths.ui
	${TS_FILES}
)
target_include_directories(BletchMAME_core PRIVATE src lib)
target_link_libraries(BletchMAME_core PRIVATE Qt5::Widgets)


#############################################################################
# BletchMAME executable                                                     #
#############################################################################

add_executable(BletchMAME src/main.cpp)
target_include_directories(BletchMAME PRIVATE src lib)
target_link_libraries(BletchMAME PRIVATE BletchMAME_core Qt5::Widgets ${EXPAT_LIBRARIES})


#############################################################################
# BletchMAME unit tests                                                     #
#############################################################################

add_executable(BletchMAME_tests	
	src/tests/test.cpp
	src/tests/client_test.cpp
	src/tests/info_builder_test.cpp
	src/tests/mameversion_test.cpp
	src/tests/prefs_test.cpp
	src/tests/runmachinetask_test.cpp
	src/tests/softwarelist_test.cpp
	src/tests/utility_test.cpp
	src/tests/xmlparser_test.cpp
	src/tests/dialogs/paths_test.cpp
	src/tests/resources.qrc
)
add_test(NAME BletchMAME_tests COMMAND BletchMAME_tests)
target_include_directories(BletchMAME_tests PRIVATE src lib src/test)
target_link_libraries(BletchMAME_tests PRIVATE BletchMAME_core Qt5::Widgets Qt5::Test ${EXPAT_LIBRARIES})


#############################################################################
# Translations                                                              #
#############################################################################

qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
