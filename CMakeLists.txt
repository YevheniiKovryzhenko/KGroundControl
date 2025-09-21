cmake_minimum_required(VERSION 3.16)

# Use modern policy so CMAKE_MSVC_RUNTIME_LIBRARY works as intended (CMake >= 3.15)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(KGroundControl VERSION 1.3 LANGUAGES CXX)

set(QT_TARGET_VERSION 6.9.2)
# set(QT_TARGET_VERSION 6.7.2)

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

# Add cmake modules path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Cross-platform Qt path detection
if(WIN32)
    # Try common Windows Qt installation paths
    set(QT_PATHS
        "$ENV{QTDIR}"
        # Static Qt installations
        "C:/Qt_Static/v${QT_TARGET_VERSION}/install"
        # Dynamic Qt installations (fallback)
        "C:/Qt_Shared/v${QT_TARGET_VERSION}/install"        
        # Standard dynamic Qt installations (fallback)
        "C:/Qt/${QT_TARGET_VERSION}/msvc2022_64"
        "D:/Qt/${QT_TARGET_VERSION}/msvc2022_64"
        "C:/Qt/${QT_TARGET_VERSION}/msvc2019_64"
        "D:/Qt/${QT_TARGET_VERSION}/msvc2019_64"
        # MinGW installations (last resort)
        "C:/Qt/${QT_TARGET_VERSION}/mingw_64"
        "D:/Qt/${QT_TARGET_VERSION}/mingw_64"
    )
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

# Remove legacy deployer variables and complex packaging; we'll keep packaging simple.

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

# If building Debug and using static Qt, optionally align MSVC runtime with Release if no debug Qt libs exist
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND (FORCE_QT_STATIC OR QT_STATIC_BUILD))
    # Heuristic: look for a debug-suffixed Qt6Core*d.lib under prefix path
    set(_qt_debug_core_lib_found FALSE)
    if(CMAKE_PREFIX_PATH)
        file(GLOB _qt_core_debug_candidates
            LIST_DIRECTORIES FALSE
            "${CMAKE_PREFIX_PATH}/lib/Qt6Cored.lib" "${CMAKE_PREFIX_PATH}/lib/Qt6Core_debug.lib" "${CMAKE_PREFIX_PATH}/lib/Qt6Core*d.lib")
        if(_qt_core_debug_candidates)
            set(_qt_debug_core_lib_found TRUE)
        endif()
    endif()
    if(NOT _qt_debug_core_lib_found)
        message(WARNING "No debug static Qt core library found. Forcing /MD and _ITERATOR_DEBUG_LEVEL=0 in Debug to avoid LNK2038 with release-only Qt.")
        # Force multi-threaded DLL runtime (Release) for all configs so this target matches Qt objects
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
        # Remove /MDd from existing flags (CMake might have already applied it before policy CMP0091)
        foreach(_flag_var CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG)
            if(${_flag_var} MATCHES "/MDd")
                string(REGEX REPLACE "/MDd" "/MD" ${_flag_var} "${${_flag_var}}")
            endif()
        endforeach()
        # Disable iterator debugging for this target to match release Qt libs (iterator debug level 0)
        add_compile_definitions($<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)
    endif()
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
    include/plotting_manager.h
    include/plot_signal_registry.h
    include/plot_canvas.h
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
    src/plotting_manager.cpp
    src/plot_signal_registry.cpp
    src/plot_canvas.cpp
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
    src/plotting_manager.ui
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

# Unify output directories across platforms
set_target_properties(KGroundControl PROPERTIES
    # Place executables and shared libs in build/bin
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"

    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"

    # Place static libraries in build/lib
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/lib"
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

# 1) Always make debugging easy on Windows dynamic builds: copy Qt DLLs into the build folder next to the .exe
if(WIN32 AND NOT (FORCE_QT_STATIC OR QT_STATIC_BUILD))
    # Try to locate windeployqt from common Qt locations or PATH
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt.exe windeployqt
        HINTS
            "${QT_PATH}/bin"
            "$ENV{QTDIR}/bin"
    )
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET KGroundControl POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                "$<TARGET_FILE:KGroundControl>"
                --dir "$<TARGET_FILE_DIR:KGroundControl>"
                --no-compiler-runtime
            COMMENT "Deploying Qt runtime DLLs to build dir for debugging"
            VERBATIM
        )
    else()
        message(STATUS "windeployqt not found. Qt runtime DLLs will not be auto-copied to the build folder.")
    endif()
endif()

# 2) For Release/MinSizeRel, create a simple DistributionKit layout automatically
#    Output: <ProjectRoot>/DistributionKit/<CONFIG>/<Static|Shared>
set(_IS_STATIC_GENEX "$<OR:$<BOOL:${FORCE_QT_STATIC}>,$<BOOL:${QT_STATIC_BUILD}>>")
set(_STATIC_OR_SHARED "$<IF:${_IS_STATIC_GENEX},Static,Shared>")
# Determine config name at build time; fall back to CMAKE_BUILD_TYPE for single-config generators
set(_CONFIG_GENEX "$<IF:$<STREQUAL:$<CONFIG>,>,${CMAKE_BUILD_TYPE},$<CONFIG>>")
set(_DEST_KIT "${PROJECT_SOURCE_DIR}/DistributionKit/${_CONFIG_GENEX}/${_STATIC_OR_SHARED}")

add_custom_command(TARGET KGroundControl POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DTARGET_FILE=$<TARGET_FILE:KGroundControl>
        -DTARGET_FILE_DIR=$<TARGET_FILE_DIR:KGroundControl>
        -DDEST_DIR=${_DEST_KIT}
        -DIS_STATIC=${_IS_STATIC_GENEX}
        -DIS_WINDOWS=$<IF:$<PLATFORM_ID:Windows>,1,0>
        -DWINDEPLOYQT_EXECUTABLE=${WINDEPLOYQT_EXECUTABLE}
        -DCONFIG=${_CONFIG_GENEX}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PackageDist.cmake
    COMMENT "Packaging KGroundControl into DistributionKit for Release/MinSizeRel"
)



