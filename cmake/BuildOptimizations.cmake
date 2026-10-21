# CMake's compiler defaults keep Release focused on speed (-O3 or /O2),
# and MinSizeRel focused on code size (-Os or /O1). Do not mix these flags.
option(KGC_ENABLE_IPO "Enable supported release link-time optimization" ON)
if(KGC_ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ipo_supported OUTPUT _ipo_error LANGUAGES CXX)
    if(_ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
    else()
        message(STATUS "Link-time optimization unavailable: ${_ipo_error}")
    endif()
endif()

# Let the linker discard unused code/data, including in our static dependencies.
set(_kgc_optimized "$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>")
if(MSVC)
    add_compile_options("$<${_kgc_optimized}:/Gy>" "$<${_kgc_optimized}:/Gw>")
    add_link_options("$<${_kgc_optimized}:/OPT:REF>" "$<${_kgc_optimized}:/OPT:ICF>"
        "$<${_kgc_optimized}:/INCREMENTAL:NO>")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT APPLE)
    add_compile_options("$<${_kgc_optimized}:-ffunction-sections>"
        "$<${_kgc_optimized}:-fdata-sections>")
    add_link_options("$<${_kgc_optimized}:-Wl,--gc-sections>")
endif()
