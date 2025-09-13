project(MAVLINK)

add_library(mavlink INTERFACE)
target_include_directories(mavlink INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/mavlink2/)

# Only apply GCC-specific warning flags when not using MSVC
if(NOT MSVC)
    target_compile_options(mavlink INTERFACE -Wno-address-of-packed-member)
endif()
