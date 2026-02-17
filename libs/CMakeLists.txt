#Libs folder
# This file manages all external and internal libraries used in KGroundControl

# Include the CMake module for MAVLink header generation
include(MavlinkGenerate)

# Set up paths for MAVLink
set(MAVLINK_REPO "${CMAKE_CURRENT_SOURCE_DIR}/Mavlink")
set(MAVLINK_GENERATED_DIR "${CMAKE_BINARY_DIR}/mavlink_generated")

# Generate MAVLink headers from message definitions
# Headers are generated into the build folder and cleaned with the build
#
# Usage:
#   cmake --preset "Release (Linux)"           # Normal: generate headers once
#   cmake -DMAVLINK_REGENERATE=ON --fresh      # Force regenerate all headers
#   rm -rf build/Release && cmake ...           # Clean build regenerates automatically
generate_mavlink_headers(
    "${MAVLINK_REPO}"
    "${MAVLINK_GENERATED_DIR}"
)

# Create Mavlink INTERFACE library (headers only, no compilation needed)
add_library(mavlink INTERFACE)
# Points to generated headers in the build folder
target_include_directories(mavlink INTERFACE
    "${MAVLINK_GENERATED_DIR}"  # Root directory containing all generated dialects
)
# Suppress compiler warning about packed structures in MAVLink headers
if(NOT MSVC)
    target_compile_options(mavlink INTERFACE -Wno-address-of-packed-member)
endif()

# Build Optitrack library
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/optitrack/") #OPTITRACK

# Note: SDL2 is now fetched from FetchContent in the main CMakeLists.txt