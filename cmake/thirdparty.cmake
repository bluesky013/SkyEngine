set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ENGINE_ROOT}/cmake/thirdparty)
include(${ENGINE_ROOT}/cmake/thirdparty_helpers.cmake)

# ---------------------------------------------------------------------------
# Detect which mode to use:
#   1. vcpkg   – when CMAKE_TOOLCHAIN_FILE points at the vcpkg toolchain
#   2. legacy  – when 3RD_PATH is set (pre-built tree from python/third_party.py)
# ---------------------------------------------------------------------------
if (DEFINED VCPKG_TOOLCHAIN OR SKY_USE_VCPKG)
    message(STATUS "[SkyEngine] Using vcpkg for third-party dependencies")
    include(${ENGINE_ROOT}/cmake/vcpkg.cmake)
elseif(EXISTS ${3RD_PATH})
    function(sky_find_3rd)
        cmake_parse_arguments(TMP
            ""
            "TARGET"
            "DIR"
            ${ARGN}
            )
        message("find target ${TMP_TARGET}")
        set(${TMP_TARGET}_PATH ${3RD_PATH}/${TMP_DIR})
        find_package(${TMP_TARGET})
        if (NOT ${${TMP_TARGET}_FOUND})
            message(FATAL_ERROR "${TMP_TARGET} not found")
        endif()
    endfunction()

    # core
    sky_find_3rd(TARGET sfmt          DIR sfmt)
    sky_find_3rd(TARGET boost         DIR boost)
    sky_find_3rd(TARGET taskflow      DIR taskflow)

    # framework
    sky_find_3rd(TARGET rapidjson     DIR rapidjson)
    sky_find_3rd(TARGET sdl           DIR sdl)

    # vulkan
    sky_find_3rd(TARGET vma           DIR vma)

    # imgui
    sky_find_3rd(TARGET imgui         DIR imgui)

    # test
    sky_find_3rd(TARGET googletest    DIR googletest)

    if (SKY_USE_TRACY)
        sky_find_3rd(TARGET tracy         DIR tracy)
        add_definitions(-DTRACY_ENABLE)
    endif ()
else()
    message(FATAL_ERROR
        "No third-party source configured.\n"
        "Either:\n"
        "  1. Use vcpkg: cmake -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake\n"
        "  2. Use pre-built: cmake -B build -D3RD_PATH=<path-to-prebuilt-3rdparty>\n"
    )
endif()
