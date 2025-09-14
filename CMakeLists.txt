cmake_minimum_required(VERSION 3.16)

# Use modern policy so CMAKE_MSVC_RUNTIME_LIBRARY works as intended (CMake >= 3.15)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(KGroundControl VERSION 1.3 LANGUAGES CXX)

set(QT_TARGET_VERSION 6.9.2)

# Ensure standard multi-config types exist when using a multi-config generator (e.g., MSVC/Visual Studio, Ninja Multi-Config)
if(MSVC OR CMAKE_GENERATOR MATCHES "Multi-Config")
    if(NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel" CACHE STRING "Available build types" FORCE)
    endif()
endif()

# For single-config generators (e.g., Ninja), set a sensible default if not provided
if(NOT DEFINED CMAKE_CONFIGURATION_TYPES AND NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Option to force static Qt build (optional; usually auto-detected)
option(FORCE_QT_STATIC "Force static Qt build even if dynamic is available" OFF)

# Configure consistent MSVC runtime library usage
if(MSVC)
    if(FORCE_QT_STATIC OR QT_STATIC_BUILD)
        # For static Qt builds, use static runtime to avoid conflicts
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
        message(STATUS "Using static runtime for static Qt build")
    else()
        # For dynamic Qt builds, use dynamic runtime
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "" FORCE)
        message(STATUS "Using dynamic runtime for dynamic Qt build")
    endif()

    # Ensure all subprojects use the same runtime
    set(SDL_FORCE_STATIC_VCRT ON CACHE BOOL "" FORCE)

    # Disable SDL_LIBC to allow RTC removal for MSVC compatibility
    set(SDL_LIBC OFF CACHE BOOL "" FORCE)

    # Filter out GCC-specific flags that MSVC doesn't understand
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Zc:__cplusplus")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")

    # Remove GCC-specific warning flags that MSVC doesn't understand
    string(REPLACE "-Wno-address-of-packed-member" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-Wno-address-of-packed-member" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

# Add cmake modules path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Cross-platform Qt path detection
if(WIN32)
    # Try common Windows Qt installation paths
    if(MSVC)
        # For MSVC, check build type hint from VS Code CMake extension
        if(CMAKE_BUILD_TYPE_HINT STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "Debug" OR "$ENV{CMAKE_BUILD_TYPE}" STREQUAL "Debug")
            # For Debug builds, prefer dynamic Qt libraries
            set(QT_PATHS
                "$ENV{QTDIR}"
                # Dynamic Qt installations (preferred for Debug)
                "C:/Qt_Shared/v${QT_TARGET_VERSION}/install"
                # Static Qt installations (fallback for Debug)
                "C:/Qt_Static/v${QT_TARGET_VERSION}/install"
                # Standard dynamic Qt installations (fallback)
                "C:/Qt/${QT_TARGET_VERSION}/msvc2022_64"
                "C:/Qt/${QT_TARGET_VERSION}/msvc2019_64"
                # MinGW installations (last resort)
                "C:/Qt/${QT_TARGET_VERSION}/mingw_64"
            )
            message(STATUS "Debug build detected: Prioritizing dynamic Qt libraries")
        else()
            # For Release/MinSizeRel builds, prefer static Qt libraries
            set(QT_PATHS
                "$ENV{QTDIR}"
                # Static Qt installations (preferred for Release to avoid DLL dependencies)
                "C:/Qt_Static/v${QT_TARGET_VERSION}/install"
                # Dynamic Qt installations (fallback for Release)
                "C:/Qt_Shared/v${QT_TARGET_VERSION}/install"
                # Standard Qt installations (fallback)
                "C:/Qt/${QT_TARGET_VERSION}/msvc2022_64"
                "C:/Qt/${QT_TARGET_VERSION}/msvc2019_64"
                # MinGW installations (last resort)
                "C:/Qt/${QT_TARGET_VERSION}/mingw_64"
            )
            message(STATUS "Release build detected: Prioritizing static Qt libraries")
        endif()
    else()
        # For MinGW/other compilers, prefer static builds
        set(QT_PATHS
            "$ENV{QTDIR}"
            # Static Qt installations (user's actual installation)
            "C:/Qt_Static/v${QT_TARGET_VERSION}/install"
            "C:/Qt_Shared/v${QT_TARGET_VERSION}/install"
            # Standard dynamic Qt installations (fallback)            
            "C:/Qt/${QT_TARGET_VERSION}/msvc2019_64"
            "C:/Qt/${QT_TARGET_VERSION}/mingw_64"
        )
    endif()
    foreach(QT_PATH ${QT_PATHS})
        if(EXISTS "${QT_PATH}")
            message(STATUS "Found Qt installation at: ${QT_PATH}")
            set(CMAKE_PREFIX_PATH "${QT_PATH}")
            
            # Set QT_STATIC_BUILD based on the Qt installation type
            if("${QT_PATH}" MATCHES ".*Qt_Shared.*" OR "${QT_PATH}" MATCHES ".*Qt/.*" OR "${QT_PATH}" MATCHES ".*msvc.*" OR "${QT_PATH}" MATCHES ".*mingw.*")
                set(QT_STATIC_BUILD FALSE CACHE BOOL "Qt is built as shared/dynamic library" FORCE)
                message(STATUS "Using dynamic Qt libraries")
            elseif("${QT_PATH}" MATCHES ".*Qt_Static.*" OR "${QT_PATH}" MATCHES ".*static.*")
                set(QT_STATIC_BUILD TRUE CACHE BOOL "Qt is built as static library" FORCE)
                message(STATUS "Using static Qt libraries")
            endif()
            
            break()
        endif()
    endforeach()
elseif(APPLE)
    # Try common macOS Qt installation paths
    set(QT_PATHS
        "$ENV{QTDIR}"
        "/opt/homebrew/lib/cmake/Qt6"
        "/usr/local/lib/cmake/Qt6"
        "~/Qt/${QT_TARGET_VERSION}/macos"
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
        "/home/$ENV{USER}/Qt/Static/v${QT_TARGET_VERSION}/install"
        "/home/$ENV{USER}/Qt/Shared/v${QT_TARGET_VERSION}/install"
    )
    foreach(QT_PATH ${QT_PATHS})
        if(EXISTS "${QT_PATH}")
            set(CMAKE_PREFIX_PATH "${QT_PATH}")
            # Auto-detect whether this Qt is static or shared by checking for static libQt6Core.a
            if(EXISTS "${QT_PATH}/lib/libQt6Core.a")
                set(QT_STATIC_BUILD TRUE CACHE BOOL "Qt is built as static library" FORCE)
                message(STATUS "Detected static Qt at: ${QT_PATH}")
            endif()
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

# MSVC specific configuration for static Qt builds
if(MSVC)
    # Ensure C++17 standard is properly detected for Qt
    add_compile_options(/Zc:__cplusplus)
    # Suppress common warnings from Qt MOC generated code
    add_compile_options(/wd4100 /wd4189 /wd4244 /wd4267)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Location)

# Configure for static Qt builds
if(FORCE_QT_STATIC OR QT_STATIC_BUILD)
    message(STATUS "Configuring for static Qt build")
    add_definitions(-DQT_STATICPLUGIN)

    # For static builds, we need to link against all required Qt libraries
    if(WIN32)
        # Windows static linking may require additional libraries
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /INCREMENTAL:NO")
        if(MSVC)
            # MSVC specific static linking flags
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:libcmt.lib /NODEFAULTLIB:msvcrt.lib")
            # Additional flags for static Qt builds
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /FORCE:MULTIPLE")
        endif()
    endif()
else()
    message(STATUS "Configuring for dynamic Qt build")
endif()

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
    include/fake_mocap_dialog.h
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
    src/fake_mocap_dialog.cpp
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
    src/fake_mocap_dialog.ui
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

# Static Qt build additional libraries
if(FORCE_QT_STATIC OR QT_STATIC_BUILD)
    if(WIN32 AND MSVC)
        # Link against required Windows system libraries for static builds
        target_link_libraries(KGroundControl PRIVATE
            # Core Windows libraries
            kernel32.lib
            user32.lib
            gdi32.lib
            winspool.lib
            shell32.lib
            ole32.lib
            oleaut32.lib
            uuid.lib
            comdlg32.lib
            advapi32.lib
            # Network and communication
            ws2_32.lib
            wsock32.lib
            # Multimedia and UI
            winmm.lib
            imm32.lib
            # Additional system libraries
            version.lib
            dwmapi.lib
            uxtheme.lib
            # Cryptography
            crypt32.lib
            # ODBC (for Qt SQL if needed)
            odbc32.lib
            odbccp32.lib
            # Universal CRT and C runtime (for static linking)
            msvcrt.lib
            ucrt.lib
            vcruntime.lib
        )
    endif()
endif()

# Include plugins (limit for Linux static builds to avoid pulling in QtPdf, etc.)
if(UNIX AND (FORCE_QT_STATIC OR QT_STATIC_BUILD))
    # Disable default plugin auto-imports and include only the Xcb platform plugin
    # to prevent linking optional plugins such as QtPdf on static builds.
    qt_import_plugins(KGroundControl
        NO_DEFAULT
        INCLUDE
            Qt6::QXcbIntegrationPlugin
    )
endif()


# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# Guard this with proper variables and limit to Apple platforms to avoid configure errors
if(APPLE)
    # For Qt 6, use Qt6_VERSION; for Qt 5, use Qt5_VERSION. Fall back safely if not defined
    if(QT_VERSION_MAJOR EQUAL 6)
        if(DEFINED Qt6_VERSION AND Qt6_VERSION VERSION_LESS "6.1.0")
            set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.KGroundControl)
        endif()
    elseif(QT_VERSION_MAJOR EQUAL 5)
        # Qt 5 predates this auto-setting, keep explicit identifier
        set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.KGroundControl)
    endif()
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
