cmake_minimum_required(VERSION 3.5)

project(KGroundControl VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network)
find_package(Qt6 REQUIRED COMPONENTS Location)

set(PROJECT_HEADERS
    connection_manager.h
    generic_port.h
    kgroundcontrol.h
    mavlink_enum_types.h
    mavlink_inspector.h
    serial_port.h
    settings.h
    threads.h
    udp_port.h
    mocap_manager.h
    relaydialog.h
    signal_filters.h
    keybinddialog.h
    # cartography.h
    default_ui_config.h
)

set(PROJECT_SOURCES
    connection_manager.cpp
    kgroundcontrol.cpp
    main.cpp
    mavlink_inspector.cpp
    serial_port.cpp
    settings.cpp
    threads.cpp
    udp_port.cpp
    mocap_manager.cpp
    relaydialog.cpp
    signal_filters.cpp
    keybinddialog.cpp
    # cartography.cpp
    default_ui_config.cpp
)

set(PROJECT_UI
    kgroundcontrol.ui
    mavlink_inspector.ui
    mocap_manager.ui
    relaydialog.ui
    mavlink_inspector_msg.ui
    keybinddialog.ui
    # cartography.ui
)



add_subdirectory(libs) #libs/ has all of the required libraries 

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(KGroundControl
        MANUAL_FINALIZATION
        ${PROJECT_HEADERS}
        ${PROJECT_SOURCES}
        ${PROJECT_UI}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET KGroundControl APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(KGroundControl SHARED
            ${PROJECT_HEADERS}
            ${PROJECT_SOURCES}
            ${PROJECT_UI}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(KGroundControl
            ${PROJECT_HEADERS}
            ${PROJECT_SOURCES}
            ${PROJECT_UI}
        )
    endif()
endif()

# target_include_directories(KGroundControl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
# target_include_directories(KGroundControl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# target_include_directories(KGroundControl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ui")

target_link_libraries(KGroundControl PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(KGroundControl PRIVATE Qt${QT_VERSION_MAJOR}::SerialPort)
target_link_libraries(KGroundControl PRIVATE Qt${QT_VERSION_MAJOR}::Network)
# target_link_libraries(KGroundControl PRIVATE Qt${QT_VERSION_MAJOR}::Location)

# Link all the outside libraries used in this project (from libs folder):
target_link_libraries(KGroundControl PRIVATE mavlink)
target_link_libraries(KGroundControl PRIVATE optitrack)

# Inlude pluggins:
qt_import_plugins(KGroundControl INCLUDE Qt::osm)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.KGroundControl)
endif()
set_target_properties(KGroundControl PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS KGroundControl
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(KGroundControl)
endif()


#deployement
find_program(Q_MAKE_EXE qmake)

find_program(CQT_EXE cqtdeployer)

IF(NOT EXISTS ${CQT_EXE})
    message("the cqtdeployer not exits please install the cqtdeployer and run cmake again!")
    return()
endif(NOT EXISTS ${CQT_EXE})

ADD_CUSTOM_TARGET(
    deploy
    DEPENDS KGroundControl
    COMMAND cqtdeployer clear -bin ${CMAKE_BINARY_DIR} -qmake ${Q_MAKE_EXE} -libDir ${PROJECT_SOURCE_DIR} -qmlDir ${PROJECT_SOURCE_DIR} -recursiveDepth 5 qif
    COMMENT "Deploy: cqtdeployer clear -bin ${CMAKE_BINARY_DIR} -qmake ${Q_MAKE_EXE} -libDir ${PROJECT_SOURCE_DIR} -qmlDir ${PROJECT_SOURCE_DIR} -recursiveDepth 5 qif"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
