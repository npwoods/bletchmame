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
set(CMAKE_CXX_STANDARD_LIBRARIES "-static-libgcc -static-libstdc++ ${CMAKE_CXX_STANDARD_LIBRARIES} -lstdc++ -lpthread")
add_definitions(-DQUAZIP_STATIC)
endif()


#############################################################################
# Dependencies                                                              #
#############################################################################

# Threads
find_package( Threads )

# Expat
if(WIN32)
# We ship a custom FindEXPAT module to support Windows debug libraries
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules")
endif()
include(FindEXPAT)
find_package(EXPAT REQUIRED)
include_directories(${EXPAT_INCLUDE_DIR})

# ZLib
include(FindZLIB)
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})

# LZMA
find_package(LZMA REQUIRED)
include_directories(${LZMA_INCLUDE_DIR})

# Qt
find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools Test)
if (NOT USE_SHARED_LIBS)
set(QT_STATIC_LIBRARIES bz2 harfbuzz brotlidec brotlicommon ${Qt6_DIR}/../../libQt6Core5Compat.a)
endif()

# QuaZip
find_package(QuaZip-Qt6 REQUIRED)
include_directories(SYSTEM ${QuaZip-Qt6_DIR}/../../../include/QuaZip-Qt6-${QuaZip-Qt6_VERSION}/quazip)
link_directories(${QuaZip-Qt6_DIR}/../../../lib)
if(NOT DEFINED QUAZIP_LIBRARIES)
set(QUAZIP_LIBRARIES quazip1-qt6)
endif()

# Yes, we're unit testing
enable_testing(true)

# Is version.gen.h available?
if (HAS_VERSION_GEN_H)
MESSAGE(STATUS "version.gen.h AVAILABLE")
add_definitions(-DHAS_VERSION_GEN_H=1)
else()
MESSAGE(STATUS "version.gen.h UNAVAILABLE")
endif()

# Define installation directories
include(GNUInstallDirs)

# Link Time Optimization
if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported OUTPUT error)
if (lto_supported)
    message(STATUS "Link Time Optimization enabled")
    set_property(GLOBAL PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "Link Time Optimization not supported: <${error}>")
	set_property(GLOBAL PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
endif()
else()
    message(STATUS "Link Time Optimization disabled in debug builds")
	set_property(GLOBAL PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
endif()

# Configure warnings for different compilers
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC")
add_compile_options(/W3 /WX -D_CRT_SECURE_NO_WARNINGS -Wno-sometimes-uninitialized)
elseif ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-sign-compare -Wno-missing-field-initializers -Wno-unused-parameter)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
add_compile_options(/W3 /WX)
endif()

# Profiler
if (USE_PROFILER)
add_compile_options(-DUSE_PROFILER=1)
MESSAGE(STATUS "Profiling support ENABLED")
else()
add_compile_options(-DUSE_PROFILER=0)
MESSAGE(STATUS "Profiling support disabled")
endif()


#############################################################################
# BletchMAME core (library shared by executable and tests)                  #
#############################################################################

add_library(BletchMAME_core
	src/7zip.cpp
	src/7zip.h
	src/assetfinder.cpp
	src/assetfinder.h
	src/audit.cpp
	src/audit.h
	src/auditablelistitemmodel.h
	src/auditcursor.cpp
	src/auditcursor.h
	src/auditqueue.cpp
	src/auditqueue.h
	src/audittask.cpp
	src/audittask.h
	src/chd.cpp
	src/chd.h
	src/devstatusdisplay.cpp
	src/devstatusdisplay.h
	src/filedlgs.cpp
	src/filedlgs.h
	src/focuswatchinghook.cpp
	src/focuswatchinghook.h
	src/hash.cpp
	src/hash.h
	src/history.cpp
	src/history.h
	src/historywatcher.cpp
	src/historywatcher.h
	src/iconloader.cpp
	src/iconloader.h
	src/identifier.cpp
	src/identifier.h
	src/imagemenu.cpp
	src/imagemenu.h
	src/importmameinijob.cpp
	src/importmameinijob.h
	src/info.cpp
	src/info.h
	src/info_builder.cpp
	src/info_builder.h
	src/iniparser.cpp
	src/iniparser.h
	src/job.cpp
	src/job.h
	src/listxmltask.cpp
	src/listxmltask.h
	src/liveinstancetracker.cpp
	src/liveinstancetracker.h
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
	src/mametask.cpp
	src/mametask.h
	src/mameversion.cpp
	src/mameversion.h
	src/mameworkercontroller.cpp
	src/mameworkercontroller.h
	src/prefs.cpp
	src/prefs.h
	src/profile.cpp
	src/profile.h
	src/profilelistitemmodel.cpp
	src/profilelistitemmodel.h
	src/perfprofiler.cpp
	src/perfprofiler.h
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
	src/tableviewmanager.cpp
	src/tableviewmanager.h
	src/task.cpp
	src/task.h	
	src/taskdispatcher.cpp
	src/taskdispatcher.h
	src/throttler.cpp
	src/throttler.h
	src/throughputtracker.cpp
	src/throughputtracker.h
	src/utility.cpp
	src/utility.h
	src/version.h
	src/versiontask.cpp
	src/versiontask.h
	src/xmlparser.cpp
	src/xmlparser.h
	src/dialogs/about.cpp
	src/dialogs/about.h
	src/dialogs/about.ui
	src/dialogs/audit.cpp
	src/dialogs/audit.h
	src/dialogs/audit.ui
	src/dialogs/audititemmodel.cpp
	src/dialogs/audititemmodel.h
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
	src/dialogs/customizefields.cpp
	src/dialogs/customizefields.h
	src/dialogs/customizefields.ui
	src/dialogs/importmameini.cpp
	src/dialogs/importmameini.h
	src/dialogs/importmameini.ui
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
	src/dialogs/pathslistviewmodel.cpp
	src/dialogs/pathslistviewmodel.h
	src/dialogs/resetprefs.cpp
	src/dialogs/resetprefs.h
	src/dialogs/resetprefs.ui
	src/dialogs/stopwarning.cpp
	src/dialogs/stopwarning.h
	src/dialogs/stopwarning.ui
	src/dialogs/switches.cpp
	src/dialogs/switches.h
	${TS_FILES}
)
target_include_directories(BletchMAME_core PRIVATE src lib ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(BletchMAME_core PRIVATE Qt6::Widgets ${QUAZIP_LIBRARIES} ${ZLIB_LIBRARIES} ${LZMA_LIBRARIES} ${QT_STATIC_LIBRARIES})
qt_import_plugins(BletchMAME_core INCLUDE_BY_TYPE imageformats Qt::QGifPlugin Qt::QJpegPlugin EXCLUDE_BY_TYPE sqldrivers)


#############################################################################
# BletchMAME executable                                                     #
#############################################################################

add_executable(BletchMAME WIN32 src/main.cpp src/resources.qrc src/winresources.rc)
target_include_directories(BletchMAME PRIVATE src lib ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(BletchMAME PRIVATE BletchMAME_core Qt6::Widgets ${QUAZIP_LIBRARIES} ${EXPAT_LIBRARIES} ${ZLIB_LIBRARIES} ${LZMA_LIBRARIES} ${QT_STATIC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
qt_import_plugins(BletchMAME INCLUDE_BY_TYPE imageformats Qt::QGifPlugin Qt::QJpegPlugin EXCLUDE_BY_TYPE sqldrivers)
install(TARGETS BletchMAME RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})


#############################################################################
# BletchMAME unit tests                                                     #
#############################################################################

add_executable(BletchMAME_tests	
	src/tests/test.cpp
	src/tests/test.h
	src/tests/assetfinder_test.cpp
	src/tests/audit_test.cpp
	src/tests/auditcursor_test.cpp
	src/tests/auditqueue_test.cpp
	src/tests/audittask_test.cpp
	src/tests/chd_test.cpp
	src/tests/devstatusdisplay_test.cpp
	src/tests/hash_test.cpp
	src/tests/history_test.cpp
	src/tests/identifier_test.cpp
	src/tests/importmameinijob_test.cpp
	src/tests/info_builder_test.cpp
	src/tests/info_test.cpp
	src/tests/iniparser_test.cpp
	src/tests/listxmlrunner.cpp
	src/tests/listxmltask_test.cpp
	src/tests/liveinstancetracker_test.cpp
	src/tests/machinefoldertreemodel_test.cpp
	src/tests/machinelistitemmodel_test.cpp
	src/tests/mainpanel_test.cpp
	src/tests/mamerunner.cpp
	src/tests/mametask_test.cpp
	src/tests/mameversion_test.cpp
	src/tests/perfprofiler_test.cpp
	src/tests/prefs_test.cpp
	src/tests/profile_test.cpp
	src/tests/runmachinetask_test.cpp
	src/tests/softwarelist_test.cpp
	src/tests/softwarelistitemmodel_test.cpp
	src/tests/status_test.cpp
	src/tests/utility_test.cpp
	src/tests/xmlparser_test.cpp
	src/tests/dialogs/confdevmodel_test.cpp
	src/tests/dialogs/inputs_test.cpp
	src/tests/dialogs/paths_test.cpp
	src/tests/dialogs/pathslistviewmodel_test.cpp
	src/tests/resources.qrc
	src/resources.qrc
)
add_test(NAME BletchMAME_tests COMMAND BletchMAME_tests)
target_include_directories(BletchMAME_tests PRIVATE src lib src/test)
target_link_libraries(BletchMAME_tests PRIVATE BletchMAME_core Qt6::Widgets Qt6::Test ${QUAZIP_LIBRARIES} ${EXPAT_LIBRARIES} ${ZLIB_LIBRARIES} ${LZMA_LIBRARIES} ${QT_STATIC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
qt_import_plugins(BletchMAME_tests INCLUDE_BY_TYPE imageformats Qt::QGifPlugin Qt::QJpegPlugin EXCLUDE_BY_TYPE sqldrivers)


#############################################################################
# Translations                                                              #
#############################################################################

qt6_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
