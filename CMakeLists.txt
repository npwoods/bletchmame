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

find_package(Qt5 COMPONENTS Widgets LinguistTools REQUIRED)

add_executable(BletchMAME
	src/client.cpp
	src/client.h
	src/collectionviewmodel.cpp
	src/collectionviewmodel.h
	src/info.cpp
	src/info.h
	src/job.cpp
	src/job.h
	src/main.cpp
	src/mainwindow.cpp
	src/mainwindow.h
	src/mainwindow.ui
	src/mameversion.cpp
	src/mameversion.h
	src/prefs.cpp
	src/prefs.h
	src/softlistviewmodel.cpp
	src/softlistviewmodel.h
	src/softwarelist.cpp
	src/softwarelist.h
	src/utility.cpp
	src/utility.h
	src/task.cpp
	src/task.h
	src/versiontask.cpp
	src/versiontask.h
	src/validity.cpp
	src/validity.h
	src/xmlparser.cpp
	src/xmlparser.h
	src/dialogs/about.cpp
	src/dialogs/about.h
	src/dialogs/about.ui
	${TS_FILES}
)

target_link_libraries(BletchMAME PRIVATE Qt5::Widgets ${EXPAT_LIBRARIES})

qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
