cmake_minimum_required(VERSION 3.22)

# Use modern policy so CMAKE_MSVC_RUNTIME_LIBRARY works as intended (CMake >= 3.15)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

# Release tags provide the version without editing source on
# main and every release branch for each patch tag.
set(KGC_VERSION "" CACHE STRING "Release version override (otherwise use exact Git tag)")
set(_kgc_version "${KGC_VERSION}")
if(NOT _kgc_version)
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --git-common-dir
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE _kgc_git_dir OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        get_filename_component(_kgc_git_dir "${_kgc_git_dir}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        file(GLOB_RECURSE _kgc_tag_files CONFIGURE_DEPENDS "${_kgc_git_dir}/refs/tags/*")
        foreach(_git_metadata "${_kgc_git_dir}/HEAD" "${_kgc_git_dir}/packed-refs" ${_kgc_tag_files})
            if(EXISTS "${_git_metadata}")
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_git_metadata}")
            endif()
        endforeach()
        execute_process(COMMAND "${GIT_EXECUTABLE}" describe --tags --exact-match
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE _kgc_tag OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(_kgc_tag MATCHES "^v?([0-9]+\\.[0-9]+\\.[0-9]+)$")
            set(_kgc_version "${CMAKE_MATCH_1}")
        endif()
        if(NOT _kgc_version)
            execute_process(COMMAND "${GIT_EXECUTABLE}" tag --merged HEAD
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                OUTPUT_VARIABLE _kgc_tags OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
            string(REPLACE "\n" ";" _kgc_tags "${_kgc_tags}")
            foreach(_tag IN LISTS _kgc_tags)
                if(_tag MATCHES "^v?([0-9]+\\.[0-9]+\\.[0-9]+)$")
                    if(NOT _kgc_version OR CMAKE_MATCH_1 VERSION_GREATER _kgc_version)
                        set(_kgc_version "${CMAKE_MATCH_1}")
                    endif()
                endif()
            endforeach()
        endif()
    endif()
endif()
if(NOT _kgc_version)
    message(FATAL_ERROR "No stable Git version tag found. Fetch tags or set KGC_VERSION when building a source archive.")
endif()
if(NOT _kgc_version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "KGC_VERSION must be a stable major.minor.patch version")
endif()
project(KGroundControl VERSION ${_kgc_version} LANGUAGES CXX)


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

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Set AUTOUIC output directory to build directory to keep source directory clean
set(CMAKE_AUTOUIC_OUTPUT_DIR ${CMAKE_BINARY_DIR}/ui_autogen)
set(CMAKE_AUTOMOC_OUTPUT_DIR ${CMAKE_BINARY_DIR}/moc_autogen)
set(CMAKE_AUTORCC_OUTPUT_DIR ${CMAKE_BINARY_DIR}/rcc_autogen)

# Set search path for UI files (moved to ui/ directory)
set(CMAKE_AUTOUIC_SEARCH_PATHS ui)

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

# Keep qDebug output in Debug and suppress it in optimized builds.
# qInfo/qWarning remain available; DEBUG supports existing conditional code.
add_compile_definitions(
    $<$<CONFIG:Debug>:DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT>
)

# AddressSanitizer for Debug builds on Linux — finds heap corruption precisely.
# Set ASAN=OFF on the cmake configure line to disable: cmake -DASAN=OFF ...
if(NOT DEFINED ASAN)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND UNIX AND NOT APPLE AND NOT MSVC)
        set(ASAN ON)
    else()
        set(ASAN OFF)
    endif()
endif()
if(ASAN)
    message(STATUS "AddressSanitizer enabled for Debug build")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

include(QtSetup)

include(BuildOptimizations)

include(FetchContent)

# Fetch SDL2 for hardware I/O and graphics
# Configure SDL2 for static compilation BEFORE declaring
set(SDL_SHARED OFF CACHE BOOL "Build SDL2 as shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL2 as static library" FORCE)
set(SDL_TEST OFF CACHE BOOL "Build SDL2 tests" FORCE)
# Keep SDL audio available alongside controller/haptic support.
set(SDL_AUDIO ON CACHE BOOL "Enable SDL audio support" FORCE)
FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.32.10
)
FetchContent_MakeAvailable(SDL2)


# Fetch PX4 ulog_cpp for official ULog writing (optional; guarded)
FetchContent_Declare(
     ulog_cpp
     GIT_REPOSITORY https://github.com/PX4/ulog_cpp.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(ulog_cpp)

# Configure for static Qt builds
if(FORCE_QT_STATIC OR QT_STATIC_BUILD)
    message(STATUS "Configuring for static Qt build")
    add_definitions(-DQT_STATICPLUGIN)
else()
    message(STATUS "Configuring for dynamic Qt build")
endif()

set(PROJECT_HEADERS
    include/kgroundcontrol.h
    include/settings.h
    include/threads.h
    include/relaydialog.h
    include/signal_filters.h
    # include/cartography.h
    include/default_ui_config.h
    include/update_manager.h
    include/update_manifest.h

    # Logging
    include/logging/log_manager.h
    
    # Hardware I/O group
    include/hardware_io/connection_manager.h
    include/hardware_io/generic_port.h
    include/hardware_io/joystick.h
    include/hardware_io/joystick_manager.h
    include/hardware_io/serial_port.h
    include/hardware_io/udp_port.h
    
    # MAVLink Communication group
    include/mavlink_communication/mavlink_enum_types.h
    include/mavlink_communication/mavlink_inspector.h
    include/mavlink_communication/remote_control_manager.h
    include/mavlink_communication/keybinddialog.h
    
    # Motion Capture group
    include/mocap/fake_mocap_dialog.h
    include/mocap/mocap_manager.h
    
    # Plot group
    include/plot/plotting_manager.h
    include/plot/plot_signal_registry.h
    include/plot/plot_signal_ui_helpers.h
    include/plot/plot_canvas.h
    include/plot/create_3d_group_dialog.h
    include/plot/sci_doublespinbox.h
)

set(PROJECT_SOURCES
    src/kgroundcontrol.cpp
    src/main.cpp
    src/settings.cpp
    src/threads.cpp
    src/relaydialog.cpp
    src/signal_filters.cpp
    src/default_ui_config.cpp
    src/update_manager.cpp

    # Logging
    src/logging/log_manager.cpp
    
    # Hardware I/O group
    src/hardware_io/connection_manager.cpp
    src/hardware_io/joystick.cpp
    src/hardware_io/joystick_manager.cpp
    src/hardware_io/serial_port.cpp
    src/hardware_io/udp_port.cpp
    
    # MAVLink Communication group
    src/mavlink_communication/mavlink_inspector.cpp
    src/mavlink_communication/remote_control_manager.cpp
    src/mavlink_communication/keybinddialog.cpp
    
    # Motion Capture group
    src/mocap/fake_mocap_dialog.cpp
    src/mocap/mocap_manager.cpp
    
    # Plot group
    src/plot/plotting_manager.cpp
    src/plot/plot_signal_registry.cpp
    src/plot/plot_signal_ui_helpers.cpp
    src/plot/plot_canvas.cpp
    src/plot/create_3d_group_dialog.cpp
)

set(PROJECT_UI
    ui/kgroundcontrol.ui
    ui/mavlink_inspector.ui
    ui/mocap_manager.ui
    ui/relaydialog.ui
    ui/mavlink_inspector_msg.ui
    ui/keybinddialog.ui
    # ui/cartography.ui
    ui/joystick_manager.ui
    ui/fake_mocap_dialog.ui
    ui/plotting_manager.ui
    ui/create_3d_group_dialog.ui
)

set(PROJECT_RESOURCES
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

# Define version information for auto-updater
target_compile_definitions(KGroundControl PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    APP_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    APP_VERSION_MINOR=${PROJECT_VERSION_MINOR}
)

target_link_libraries(KGroundControl PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(KGroundControl PRIVATE Qt${QT_VERSION_MAJOR}::SerialPort)
target_link_libraries(KGroundControl PRIVATE Qt${QT_VERSION_MAJOR}::Network)
# target_link_libraries(KGroundControl PRIVATE Qt${QT_VERSION_MAJOR}::Location)


# Link all the outside libraries used in this project (from libs folder and FetchContent):
target_link_libraries(KGroundControl PRIVATE mavlink)
target_link_libraries(KGroundControl PRIVATE optitrack)
# Do not link SDL2main when using Qt (Qt provides its own WinMain entrypoint).
# Define SDL_MAIN_HANDLED so SDL does not redefine main.
target_compile_definitions(KGroundControl PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(KGroundControl PRIVATE SDL2-static)
# Add SDL2 include directory (from FetchContent)
target_include_directories(KGroundControl PRIVATE ${sdl2_SOURCE_DIR}/include)

# Link the official PX4 ULog writer.
target_link_libraries(KGroundControl PRIVATE ulog_cpp)
# If FetchContent populated the source dir variable, add its include path so headers are available.
target_include_directories(KGroundControl PRIVATE ${ulog_cpp_SOURCE_DIR}/include)

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
        )
    endif()
endif()

# Include plugins (limit for Linux static builds to avoid pulling in QtPdf, etc.)
if(QT_VERSION_MAJOR EQUAL 6 AND UNIX AND NOT APPLE AND QT_STATIC_BUILD)
    # Disable default plugin auto-imports and include only essential plugins
    # to prevent linking optional plugins such as QtPdf on static builds.
    qt_import_plugins(KGroundControl
        NO_DEFAULT
        INCLUDE
            Qt6::QXcbIntegrationPlugin
            Qt6::QTlsBackendOpenSSLPlugin
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

# Determine OS name and architecture for versioned binary naming.
# The value produced here must match the strings used by the auto‑update
# workflow when constructing asset names.  "Windows" and "macOS" are
# hard-coded; on Linux we attempt to read /etc/os-release and produce a
# string such as "Ubuntu_24.04" or "Debian_12".  This keeps the
# DistributionKit artifact name aligned with the platform key logic used at
# runtime, while still being readable for human operators.
if(WIN32)
    set(OS_NAME "Windows")
    set(BINARY_EXTENSION ".exe")
elseif(UNIX AND NOT APPLE)
    # default fallback if detection fails
    set(OS_NAME "linux")
    set(BINARY_EXTENSION "")
    if(EXISTS "/etc/os-release")
        file(READ "/etc/os-release" _osrel)
        # anchor regexes to start of line to avoid matching VERSION_ID for ID
        string(REGEX MATCH "(^|\n)ID=\"?([^\n\"]+)\"?" _match "${_osrel}")
        set(_id "${CMAKE_MATCH_2}")
        string(REGEX MATCH "(^|\n)VERSION_ID=\"?([^\n\"]+)\"?" _match2 "${_osrel}")
        set(_ver "${CMAKE_MATCH_2}")

        # sanitize and format: capitalize ID.  version string is left
        # unchanged except for optional quoting; dots are desirable because
        # they match the naming convention used in the update JSON and
        # asset names (e.g. Ubuntu_22.04).
        string(TOLOWER "${_id}" _id_low)
        string(SUBSTRING "${_id_low}" 0 1 _first)
        string(TOUPPER "${_first}" _first_up)
        string(SUBSTRING "${_id_low}" 1 -1 _rest)
        string(CONCAT _id_fmt "${_first_up}${_rest}")
        set(_ver_fmt "${_ver}")
        set(OS_NAME "${_id_fmt}_${_ver_fmt}")
    endif()
elseif(APPLE)
    set(OS_NAME "macOS")
    set(BINARY_EXTENSION "")
else()
    set(OS_NAME "Unknown")
    set(BINARY_EXTENSION "")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$" OR CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
    set(ARCH_NAME "arm64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_NAME "x64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(ARCH_NAME "x86")
else()
    set(ARCH_NAME "unknown")
endif()

if(WIN32)
    set(_update_platform "windows-${ARCH_NAME}")
else()
    string(TOLOWER "${OS_NAME}" _update_os)
    string(REPLACE "_" "-" _update_os "${_update_os}")
    set(_update_platform "linux-${_update_os}-${ARCH_NAME}")
endif()
target_compile_definitions(KGroundControl PRIVATE
    APP_UPDATE_PLATFORM="${_update_platform}"
    APP_UPDATE_ASSET_SUFFIX="${OS_NAME}_${ARCH_NAME}${BINARY_EXTENSION}"
)

# Versioned binary name: KGroundControl_v1.2.3_Ubuntu_24.04_x64
set(VERSIONED_BINARY_NAME "KGroundControl_v${PROJECT_VERSION}_${OS_NAME}_${ARCH_NAME}${BINARY_EXTENSION}")
message(STATUS "Versioned binary name: ${VERSIONED_BINARY_NAME}")

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
            "${Qt${QT_VERSION_MAJOR}_DIR}/../../../bin"
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

# Deployment copies are separate from build/debug binaries and previous versions.
option(KGC_STRIP_DEPLOYMENT "Strip symbols from Linux/MinGW deployment copies" ON)
set(_kgc_system_runtime_libraries "")
if(MSVC AND CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    set(_kgc_system_runtime_libraries "${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}")
endif()
set(KGC_OPENSSL_RUNTIME_DIR "" CACHE PATH "Directory containing matching Windows OpenSSL runtime DLLs")
set(KGC_DEPLOY_RUNTIME_FILES "" CACHE STRING "Additional runtime files to include in deployments (semicolon-separated)")
set(_kgc_extra_runtime_files "${KGC_DEPLOY_RUNTIME_FILES}")
if(WIN32)
    set(_kgc_ssl_hints "${KGC_OPENSSL_RUNTIME_DIR}"
        "${Qt${QT_VERSION_MAJOR}_DIR}/../../../bin"
        "$ENV{OPENSSL_ROOT_DIR}/bin" "$ENV{ProgramFiles}/OpenSSL-Win64/bin")
    find_file(_kgc_ssl_dll NAMES libssl-3-x64.dll libssl-3.dll HINTS ${_kgc_ssl_hints} NO_CACHE)
    find_file(_kgc_crypto_dll NAMES libcrypto-3-x64.dll libcrypto-3.dll HINTS ${_kgc_ssl_hints} NO_CACHE)
    if(_kgc_ssl_dll AND _kgc_crypto_dll)
        list(APPEND _kgc_extra_runtime_files "${_kgc_ssl_dll}" "${_kgc_crypto_dll}")
    else()
        message(STATUS "OpenSSL DLLs not found for deployment; set KGC_OPENSSL_RUNTIME_DIR to bundle them")
    endif()
endif()
set(_STATIC_OR_SHARED "$<IF:$<BOOL:${QT_STATIC_BUILD}>,Static,Shared>")
set(_DEST_KIT "${PROJECT_SOURCE_DIR}/DistributionKit/$<CONFIG>/${OS_NAME}/${_STATIC_OR_SHARED}/v${PROJECT_VERSION}")
set(_kgc_bundle "")
if(APPLE)
    set(_kgc_bundle "$<TARGET_BUNDLE_DIR:KGroundControl>")
endif()
# A generated input file preserves list arguments and paths containing spaces.
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/deployment-$<CONFIG>.cmake" CONTENT "
set(PROJECT_SOURCE_DIR [==[${PROJECT_SOURCE_DIR}]==])
set(TARGET_FILE [==[$<TARGET_FILE:KGroundControl>]==])
set(APP_BUNDLE_DIR [==[${_kgc_bundle}]==])
set(DEST_DIR [==[${_DEST_KIT}]==])
set(CONFIG [==[$<CONFIG>]==])
set(VERSIONED_BINARY_NAME [==[${VERSIONED_BINARY_NAME}]==])
set(IS_STATIC [==[${QT_STATIC_BUILD}]==])
set(IS_WINDOWS [==[${WIN32}]==])
set(IS_APPLE [==[${APPLE}]==])
set(IS_MSVC [==[${MSVC}]==])
set(STRIP_DEPLOYMENT [==[${KGC_STRIP_DEPLOYMENT}]==])
set(STRIP_EXECUTABLE [==[${CMAKE_STRIP}]==])
set(WINDEPLOYQT_EXECUTABLE [==[${WINDEPLOYQT_EXECUTABLE}]==])
set(SYSTEM_RUNTIME_LIBRARIES [==[${_kgc_system_runtime_libraries}]==])
set(EXTRA_RUNTIME_FILES [==[${_kgc_extra_runtime_files}]==])
set(BUILD_DESCRIPTION [==[Project: ${PROJECT_NAME} ${PROJECT_VERSION}
Configuration: $<CONFIG>
Qt: ${Qt${QT_VERSION_MAJOR}_VERSION}
Static Qt: ${QT_STATIC_BUILD}
Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}
Architecture: ${ARCH_NAME}
Platform: ${OS_NAME}
Release C++ flags: ${CMAKE_CXX_FLAGS_RELEASE}
Minimum Size Release C++ flags: ${CMAKE_CXX_FLAGS_MINSIZEREL}
Link-time optimization requested: ${KGC_ENABLE_IPO}
Link-time optimization enabled: $<BOOL:$<TARGET_PROPERTY:KGroundControl,INTERPROCEDURAL_OPTIMIZATION_$<UPPER_CASE:$<CONFIG>>>>
Strip deployment requested: ${KGC_STRIP_DEPLOYMENT}
]==])
")
set(_kgc_deploy_command "${CMAKE_COMMAND}"
    "-DDEPLOYMENT_INPUT=${CMAKE_CURRENT_BINARY_DIR}/deployment-$<CONFIG>.cmake"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PackageDist.cmake")
add_custom_command(TARGET KGroundControl POST_BUILD
    COMMAND ${_kgc_deploy_command}
    COMMENT "Preparing deployment for optimized builds"
    VERBATIM
)
# Explicit refresh also updates documentation/manifests when no relink is needed.
add_custom_target(deployment
    COMMAND ${_kgc_deploy_command}
    DEPENDS KGroundControl
    COMMENT "Refreshing deployment package"
    VERBATIM
)
