cmake_minimum_required(VERSION 3.5)

project(KGroundControl VERSION 1.3 LANGUAGES CXX)

# Add cmake modules path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Cross-platform Qt path detection
if(WIN32)
    # Try common Windows Qt installation paths
    set(QT_PATHS
        "$ENV{QTDIR}"
        "C:/Qt/6.9.2/msvc2019_64"
        "C:/Qt/6.9.2/mingw_64"
        "C:/Qt/6.7.2/msvc2019_64"
        "C:/Qt/6.7.2/mingw_64"
    )
    foreach(QT_PATH ${QT_PATHS})
        if(EXISTS "${QT_PATH}")
            set(CMAKE_PREFIX_PATH "${QT_PATH}")
            break()
        endif()
    endforeach()
elseif(APPLE)
    # Try common macOS Qt installation paths
    set(QT_PATHS
        "$ENV{QTDIR}"
        "/opt/homebrew/lib/cmake/Qt6"
        "/usr/local/lib/cmake/Qt6"
        "~/Qt/6.9.2/macos"
        "~/Qt/6.7.2/macos"
    )
    foreach(QT_PATH ${QT_PATHS})
        if(EXISTS "${QT_PATH}")
            set(CMAKE_PREFIX_PATH "${QT_PATH}")
            break()
        endif()
    endforeach()
else() # Linux and other Unix-like systems
    # Try common Linux Qt installation paths
    set(QT_PATHS
        "$ENV{QTDIR}"
        "/home/$ENV{USER}/Qt/v6.9.2/install-static"
        "/home/$ENV{USER}/Qt/6.9.2/gcc_64"
        "/home/$ENV{USER}/Qt/6.7.2/install-static"
        "/home/$ENV{USER}/Qt/6.7.2/gcc_64"
        "/opt/qt6-static"
        "/usr/lib/cmake/Qt6"
    )
    foreach(QT_PATH ${QT_PATHS})
        if(EXISTS "${QT_PATH}")
            set(CMAKE_PREFIX_PATH "${QT_PATH}")
            break()
        endif()
    endforeach()
endif()

# If no Qt path found, let CMake find it automatically
if(NOT CMAKE_PREFIX_PATH)
    message(STATUS "No Qt installation found in common paths. CMake will search automatically.")
endif()

# Set the download URL based on the platform
if(WIN32)
    set(CQTDEPLOYER_URL "https://github.com/QuasarApp/CQtDeployer/releases/download/v1.6.2286/CQtDeployer_1.6.2286_Windows_x86_64.zip")
    set(CQTDEPLOYER_ARCHIVE "CQtDeployer.zip")
    set(CQTDEPLOYER_EXE "cqtdeployer.exe")
elseif(APPLE)
    set(CQTDEPLOYER_URL "https://github.com/QuasarApp/CQtDeployer/releases/download/v1.6.2286/CQtDeployer_1.6.2286_MacOS_x86_64.dmg")
    set(CQTDEPLOYER_ARCHIVE "CQtDeployer.dmg")
    set(CQTDEPLOYER_EXE "cqtdeployer")
else() # Linux
    set(CQTDEPLOYER_URL "https://github.com/QuasarApp/CQtDeployer/releases/download/v1.6.2286/CQtDeployer_1.6.2286_Linux_x86_64.deb")
    set(CQTDEPLOYER_ARCHIVE "CQtDeployer.deb")
    set(CQTDEPLOYER_EXE "cqtdeployer")
endif()

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Set AUTOUIC output directory to build directory to keep source directory clean
set(CMAKE_AUTOUIC_OUTPUT_DIR ${CMAKE_BINARY_DIR}/ui_autogen)
set(CMAKE_AUTOMOC_OUTPUT_DIR ${CMAKE_BINARY_DIR}/moc_autogen)
set(CMAKE_AUTORCC_OUTPUT_DIR ${CMAKE_BINARY_DIR}/rcc_autogen)

# Suppress CMake developer warnings
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE INTERNAL "Suppress CMake developer warnings")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Location)

set(PROJECT_HEADERS
    include/connection_manager.h
    include/generic_port.h
    include/kgroundcontrol.h
    include/mavlink_enum_types.h
    include/mavlink_inspector.h
    include/serial_port.h
    include/settings.h
    include/threads.h
    include/udp_port.h
    include/mocap_manager.h
    include/relaydialog.h
    include/signal_filters.h
    include/keybinddialog.h
    # include/cartography.h
    include/default_ui_config.h
    include/joystick.h
    include/joystick_manager.h
    include/remote_control_manager.h
)

set(PROJECT_SOURCES
    src/connection_manager.cpp
    src/kgroundcontrol.cpp
    src/main.cpp
    src/mavlink_inspector.cpp
    src/serial_port.cpp
    src/settings.cpp
    src/threads.cpp
    src/udp_port.cpp
    src/mocap_manager.cpp
    src/relaydialog.cpp
    src/signal_filters.cpp
    src/keybinddialog.cpp
    # src/cartography.cpp
    src/default_ui_config.cpp
    src/joystick.cpp
    src/joystick_manager.cpp
    src/remote_control_manager.cpp
)

set(PROJECT_UI
    src/kgroundcontrol.ui
    src/mavlink_inspector.ui
    src/mocap_manager.ui
    src/relaydialog.ui
    src/mavlink_inspector_msg.ui
    src/keybinddialog.ui
    # src/cartography.ui
    src/joystick_manager.ui
)

set(PROJECT_RESOURCES
    resources/SDL/SDL.qrc
    resources/Images/Images.qrc
)

add_subdirectory(libs) #libs/ has all of the required libraries

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)

    if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")

        set(app_icon_resource_windows  "${CMAKE_CURRENT_SOURCE_DIR}/resources/Images/Logo/KLogo.rc")

        qt_add_executable(KGroundControl
            MANUAL_FINALIZATION
            ${PROJECT_HEADERS}
            ${PROJECT_SOURCES}
            ${PROJECT_UI}
            ${PROJECT_RESOURCES}
            ${app_icon_resource_windows}
        )

    else()
        qt_add_executable(KGroundControl
            MANUAL_FINALIZATION
            ${PROJECT_HEADERS}
            ${PROJECT_SOURCES}
            ${PROJECT_UI}
            ${PROJECT_RESOURCES}
        )
    endif()


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

target_include_directories(KGroundControl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(KGroundControl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# Include build directory for auto-generated UI headers
target_include_directories(KGroundControl PRIVATE "${CMAKE_BINARY_DIR}/ui_autogen")

target_link_libraries(KGroundControl PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(KGroundControl PRIVATE Qt${QT_VERSION_MAJOR}::SerialPort)
target_link_libraries(KGroundControl PRIVATE Qt${QT_VERSION_MAJOR}::Network)
# target_link_libraries(KGroundControl PRIVATE Qt${QT_VERSION_MAJOR}::Location)

# Link all the outside libraries used in this project (from libs folder):
target_link_libraries(KGroundControl PRIVATE mavlink)
target_link_libraries(KGroundControl PRIVATE optitrack)
target_link_libraries(KGroundControl PRIVATE SDL2main SDL2-static)

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


# AppImage creation target
find_program(LINUXDEPLOYQT_EXECUTABLE linuxdeployqt HINTS $ENV{HOME}/.local/bin $ENV{HOME}/.local/bin/ /usr/local/bin)
if(LINUXDEPLOYQT_EXECUTABLE)
    message(STATUS "Found linuxdeployqt: ${LINUXDEPLOYQT_EXECUTABLE}")
    ADD_CUSTOM_TARGET(
        appimage
        DEPENDS KGroundControl
        # Create AppDir structure
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/AppDir/usr/bin
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/AppDir/usr/share/applications
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/AppDir/usr/share/icons/hicolor/256x256/apps
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/AppDir/usr/share/KGroundControl
        # Copy executable
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/KGroundControl ${CMAKE_BINARY_DIR}/AppDir/usr/bin/
        # Copy resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/resources ${CMAKE_BINARY_DIR}/AppDir/usr/share/KGroundControl/
        # Create desktop file using CMake file(WRITE)
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_BINARY_DIR}/AppDir/usr/share/applications/KGroundControl.desktop
        COMMAND ${CMAKE_COMMAND} -D DESKTOP_FILE=${CMAKE_BINARY_DIR}/AppDir/usr/share/applications/KGroundControl.desktop -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteDesktop.cmake
        # Copy icon (using existing logo)
        COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/resources/Images/Logo/KGC_Logo.png ${CMAKE_BINARY_DIR}/AppDir/usr/share/icons/hicolor/256x256/apps/KGroundControl.png
        # Create AppRun script using CMake file(WRITE)
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_BINARY_DIR}/AppDir/AppRun
        COMMAND ${CMAKE_COMMAND} -D APP_RUN_FILE=${CMAKE_BINARY_DIR}/AppDir/AppRun -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteAppRun.cmake
        COMMAND chmod +x ${CMAKE_BINARY_DIR}/AppDir/AppRun
        # Use linuxdeployqt to create AppImage with proper version
        COMMAND ${CMAKE_COMMAND} -E env VERSION=${PROJECT_VERSION} ${LINUXDEPLOYQT_EXECUTABLE} ${CMAKE_BINARY_DIR}/AppDir/usr/share/applications/KGroundControl.desktop -appimage -no-translations
        # Create distribution directory and copy AppImage
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/AppImage
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/*.AppImage ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/AppImage/
        # Copy documentation files
        COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/docs/distribution/templates/AppImage/README.md ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/AppImage/
        COMMENT "Creating AppImage for KGroundControl and copying to DistributionKit"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
else()
    message(STATUS "linuxdeployqt not found - AppImage target will not be available")
endif()

# DEB package creation target
find_program(DPKG_EXECUTABLE dpkg)
if(DPKG_EXECUTABLE)
    ADD_CUSTOM_TARGET(
        deb
        DEPENDS KGroundControl
        # Create DEB directory structure
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/deb/KGroundControl-${PROJECT_VERSION}/DEBIAN
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/deb/KGroundControl-${PROJECT_VERSION}/usr/bin
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/deb/KGroundControl-${PROJECT_VERSION}/usr/share/applications
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/deb/KGroundControl-${PROJECT_VERSION}/usr/share/icons/hicolor/256x256/apps
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/deb/KGroundControl-${PROJECT_VERSION}/usr/share/KGroundControl
        # Copy executable
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/KGroundControl ${CMAKE_BINARY_DIR}/deb/KGroundControl-${PROJECT_VERSION}/usr/bin/
        # Copy resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/resources ${CMAKE_BINARY_DIR}/deb/KGroundControl-${PROJECT_VERSION}/usr/share/KGroundControl/
        # Create control file using CMake file(WRITE) for reliability
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_BINARY_DIR}/deb/KGroundControl-${PROJECT_VERSION}/DEBIAN/control
        COMMAND ${CMAKE_COMMAND} -D CONTROL_FILE=${CMAKE_BINARY_DIR}/deb/KGroundControl-${PROJECT_VERSION}/DEBIAN/control -D PROJECT_VERSION=${PROJECT_VERSION} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteControl.cmake
        # Create desktop file using CMake file(WRITE)
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_BINARY_DIR}/deb/KGroundControl-${PROJECT_VERSION}/usr/share/applications/KGroundControl.desktop
        COMMAND ${CMAKE_COMMAND} -D DESKTOP_FILE=${CMAKE_BINARY_DIR}/deb/KGroundControl-${PROJECT_VERSION}/usr/share/applications/KGroundControl.desktop -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteDesktop.cmake
        # Copy icon
        COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/resources/Images/Logo/KGC_Logo.png ${CMAKE_BINARY_DIR}/deb/KGroundControl-${PROJECT_VERSION}/usr/share/icons/hicolor/256x256/apps/KGroundControl.png
        # Build DEB package
        COMMAND dpkg-deb --build ${CMAKE_BINARY_DIR}/deb/KGroundControl-${PROJECT_VERSION} ${CMAKE_BINARY_DIR}/KGroundControl-${PROJECT_VERSION}.deb
        # Create distribution directory and copy DEB
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/DEB
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/KGroundControl-${PROJECT_VERSION}.deb ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/DEB/
        # Copy documentation files
        COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/docs/distribution/templates/DEB/README.md ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/DEB/
        COMMENT "Creating DEB package for KGroundControl and copying to DistributionKit"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

# Simple tar.gz archive target
ADD_CUSTOM_TARGET(
    tarball
    DEPENDS KGroundControl
    # Create architecture-specific directory structure first
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/bin
    # Copy executable
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/KGroundControl ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/bin/
    # Create architecture info file
    COMMAND ${CMAKE_COMMAND} -E echo "Architecture: ${CMAKE_SYSTEM_PROCESSOR}" > ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/ARCHITECTURE.txt
    COMMAND ${CMAKE_COMMAND} -E echo "Build Date: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}" >> ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/ARCHITECTURE.txt
    COMMAND ${CMAKE_COMMAND} -E echo "CMake Generator: ${CMAKE_GENERATOR}" >> ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/ARCHITECTURE.txt
    COMMAND ${CMAKE_COMMAND} -E echo "Build Type: ${CMAKE_BUILD_TYPE}" >> ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/ARCHITECTURE.txt
    COMMAND ${CMAKE_COMMAND} -E echo "Qt Version: ${QT_VERSION}" >> ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/ARCHITECTURE.txt
    COMMAND ${CMAKE_COMMAND} -E echo "Build Timestamp: " >> ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/ARCHITECTURE.txt
    COMMAND ${CMAKE_COMMAND} -DARCH_FILE=${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/ARCHITECTURE.txt -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Timestamp.cmake
    COMMAND ${CMAKE_COMMAND} -E echo "Note: This tarball contains only the executable. For resources, use the AppImage or DEB package." >> ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/ARCHITECTURE.txt
    # Create tarball from just the bin directory and ARCHITECTURE.txt
    COMMAND cd ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR} && ${CMAKE_COMMAND} -E tar czf ${CMAKE_BINARY_DIR}/KGroundControl-${PROJECT_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.tar.gz bin ARCHITECTURE.txt
    # Create distribution directory and copy tarball
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/Tarball
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/KGroundControl-${PROJECT_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.tar.gz ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/Tarball/
    # Copy documentation files
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/docs/distribution/templates/README.md ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/docs/distribution/templates/Tarball/README.md ${PROJECT_SOURCE_DIR}/DistributionKit/${CMAKE_SYSTEM_PROCESSOR}/Tarball/
    # Clean up the tarball from project root if it exists
    COMMAND ${CMAKE_COMMAND} -E rm -f ${PROJECT_SOURCE_DIR}/KGroundControl-${PROJECT_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.tar.gz
    COMMENT "Creating tar.gz archive of the deployment package and copying to DistributionKit"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

# Comprehensive distribution target that builds all available formats
ADD_CUSTOM_TARGET(
    dist
    COMMENT "Building all available distribution formats"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

# Add dependencies to the dist target based on what's available
if(LINUXDEPLOYQT_EXECUTABLE)
    add_dependencies(dist appimage)
endif()
if(DPKG_EXECUTABLE)
    add_dependencies(dist deb)
endif()
add_dependencies(dist tarball)

# Unified deploy target that builds all formats and copies documentation
ADD_CUSTOM_TARGET(
    deploy
    COMMENT "Building complete deployment package with documentation"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

# Add dependencies to the deploy target based on what's available
if(LINUXDEPLOYQT_EXECUTABLE)
    add_dependencies(deploy appimage)
endif()
if(DPKG_EXECUTABLE)
    add_dependencies(deploy deb)
endif()
add_dependencies(deploy tarball)
