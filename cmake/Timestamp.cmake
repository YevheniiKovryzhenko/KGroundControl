# Cross-platform timestamp generation for CMake
# This script appends a timestamp to the architecture file

if(WIN32)
    # Use PowerShell to get timestamp on Windows
    execute_process(
        COMMAND powershell -Command "Get-Date -Format 'yyyy-MM-dd HH:mm:ss'"
        OUTPUT_VARIABLE TIMESTAMP
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
elseif(UNIX)
    # Use date command on Unix-like systems
    execute_process(
        COMMAND date
        OUTPUT_VARIABLE TIMESTAMP
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    # Fallback for other systems
    set(TIMESTAMP "Unknown timestamp")
endif()

# Append timestamp to the architecture file
file(APPEND ${ARCH_FILE} "${TIMESTAMP}\n")