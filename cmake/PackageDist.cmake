# Simple packaging script invoked POST_BUILD
# Arguments passed via -D:
#  - PROJECT_SOURCE_DIR: project root
#  - TARGET_FILE: full path to built executable
#  - TARGET_FILE_DIR: directory of built executable
#  - DEST_DIR: destination root (DistributionKit/<CONFIG>/<Static|Shared>)
#  - IS_STATIC: generator expression result (true/false)
#  - IS_WINDOWS: 1 if Windows, 0 otherwise
#  - WINDEPLOYQT_EXECUTABLE: path to windeployqt if available
#  - CONFIG: configuration name (Debug, Release, MinSizeRel, ...)
#  - VERSIONED_BINARY_NAME: versioned name for the executable (e.g., KGroundControl_v1.2.3_Linux_x64)

if(NOT DEFINED CONFIG)
  set(CONFIG "")
endif()

# Only package for Release or MinSizeRel
if(NOT CONFIG STREQUAL "Release" AND NOT CONFIG STREQUAL "MinSizeRel")
  message(STATUS "PackageDist: Skipping packaging for config '${CONFIG}'. Only Release/MinSizeRel are packaged.")
  return()
endif()

# Prepare destination
file(MAKE_DIRECTORY "${DEST_DIR}")

# Copy the executable with versioned name and other side-by-side contents
# On Windows with dynamic Qt, the build directory should already contain Qt DLLs (from windeployqt run in main CMakeLists)
get_filename_component(_target_name "${TARGET_FILE}" NAME)
file(GLOB _build_contents "${TARGET_FILE_DIR}/*")
foreach(item IN LISTS _build_contents)
  get_filename_component(name "${item}" NAME)
  if(IS_DIRECTORY "${item}")
    file(COPY "${item}" DESTINATION "${DEST_DIR}/")
  else()
    # Rename the main executable to versioned name
    if(name STREQUAL _target_name AND DEFINED VERSIONED_BINARY_NAME)
      file(COPY "${item}" DESTINATION "${DEST_DIR}/" FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
      file(RENAME "${DEST_DIR}/${name}" "${DEST_DIR}/${VERSIONED_BINARY_NAME}")
    else()
      file(COPY "${item}" DESTINATION "${DEST_DIR}/" FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    endif()
  endif()
endforeach()

# Copy docs if present
foreach(doc IN ITEMS README.md LICENSE)
  if(EXISTS "${PROJECT_SOURCE_DIR}/${doc}")
    file(COPY "${PROJECT_SOURCE_DIR}/${doc}" DESTINATION "${DEST_DIR}/")
  endif()
endforeach()

message(STATUS "PackageDist: Packaged '${CONFIG}' into: ${DEST_DIR}")
