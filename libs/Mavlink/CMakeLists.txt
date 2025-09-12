project(MAVLINK)

add_library(mavlink INTERFACE)
target_include_directories(mavlink INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/mavlink2/)
target_compile_options(mavlink INTERFACE -Wno-address-of-packed-member)
