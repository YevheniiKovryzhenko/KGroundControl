# MavlinkGenerate.cmake
# Fetches pre-generated MAVLink C headers from the official c_library_v2 repository
# Headers are downloaded into the build folder (not stored in source)
# Automatically cleaned with the build folder on rebuild
#
# Usage:
#   cmake --preset "Release (Linux)"      # Normal: download headers if needed
#   rm -rf build/Release && cmake ...     # Clean build automatically re-downloads
#
# No external dependencies - only Git and CMake

function(generate_mavlink_headers MAVLINK_REPO_DIR OUTPUT_DIR)
    # Check if headers already downloaded/available
    set(HEADER_CHECK "${OUTPUT_DIR}/common/mavlink.h")
    
    if(EXISTS "${HEADER_CHECK}")
        message(STATUS "[MAVLink] Headers available in build folder")
        return()
    endif()
    
    message(STATUS "[MAVLink] Downloading pre-generated headers from c_library_v2...")
    
    # Find Git (required for download)
    find_package(Git QUIET REQUIRED)
    
    # Create temp directory for download
    set(TEMP_DIR "${CMAKE_BINARY_DIR}/_temp_mavlink_download")
    if(EXISTS "${TEMP_DIR}")
        file(REMOVE_RECURSE "${TEMP_DIR}")
    endif()
    file(MAKE_DIRECTORY "${TEMP_DIR}")
    
    # Clone c_library_v2 repository (shallow clone for speed)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} clone --depth 1 https://github.com/mavlink/c_library_v2.git "${TEMP_DIR}/c_lib"
        RESULT_VARIABLE GIT_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )
    
    if(NOT GIT_RESULT EQUAL 0)
        message(FATAL_ERROR "[MAVLink] Failed to download headers from GitHub. Check your internet connection.")
    endif()
    
    # Create output directory and copy all dialects
    file(MAKE_DIRECTORY "${OUTPUT_DIR}")
    
    message(STATUS "[MAVLink] Copying header files...")
    
    # Copy all dialect directories
    file(GLOB DIALECT_ITEMS "${TEMP_DIR}/c_lib/*")
    foreach(ITEM ${DIALECT_ITEMS})
        get_filename_component(ITEM_NAME "${ITEM}" NAME)
        
        # Skip git metadata and non-directories
        if(ITEM_NAME STREQUAL ".git" OR NOT IS_DIRECTORY "${ITEM}")
            # But do copy individual header files like mavlink_helpers.h
            if(ITEM MATCHES "\\.h$")
                execute_process(COMMAND ${CMAKE_COMMAND} -E copy "${ITEM}" "${OUTPUT_DIR}/${ITEM_NAME}")
            endif()
            continue()
        endif()
        
        # Copy dialect directory
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ITEM}" "${OUTPUT_DIR}/${ITEM_NAME}"
        )
        message(STATUS "[MAVLink] ✓ ${ITEM_NAME}")
    endforeach()
    
    # Clean up temp directory
    file(REMOVE_RECURSE "${TEMP_DIR}")
    
    # Verify headers were installed
    if(NOT EXISTS "${HEADER_CHECK}")
        message(FATAL_ERROR "[MAVLink] Expected ${HEADER_CHECK} not found after download")
    endif()
    
    message(STATUS "[MAVLink] ✓ Headers successfully installed to build folder")

endfunction()
