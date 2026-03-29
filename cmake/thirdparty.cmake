set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ENGINE_ROOT}/cmake/thirdparty)
include(${ENGINE_ROOT}/cmake/thirdparty_helpers.cmake)

# ---------------------------------------------------------------------------
# Detect which mode to use (configured by SKY_THIRDPARTY_MODE in top-level CMake):
#   1. LOCAL    – vcpkg manifest mode (download/build/cache under SKY_THIRDPARTY_ROOT)
#   2. PREBUILT – pre-built tree from python/third_party.py via 3RD_PATH
# ---------------------------------------------------------------------------
if (SKY_THIRDPARTY_MODE STREQUAL "LOCAL")
    message(STATUS "[SkyEngine] Third-party mode: LOCAL (vcpkg)")
    message(STATUS "[SkyEngine] Third-party root: ${SKY_THIRDPARTY_ROOT}")
    include(${ENGINE_ROOT}/cmake/vcpkg.cmake)
elseif (SKY_THIRDPARTY_MODE STREQUAL "PREBUILT" AND EXISTS ${3RD_PATH})
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

    # shader
    sky_find_3rd(TARGET glslang       DIR glslang)
    sky_find_3rd(TARGET SPIRVCross    DIR SPIRV-Cross)
    if (WIN32)
        sky_find_3rd(TARGET dxcompiler    DIR dxcompiler)
    endif ()

    # test
    sky_find_3rd(TARGET googletest    DIR googletest)

    # script
    if (SKY_BUILD_CPYTHON)
        sky_find_3rd(TARGET cpython       DIR cpython)
    endif ()

    if (SKY_BUILD_COMPRESSION)
        sky_find_3rd(TARGET lz4           DIR lz4)
    endif ()

    # text
    if (SKY_BUILD_FREETYPE)
        sky_find_3rd(TARGET freetype      DIR freetype)
    endif ()

    if (SKY_USE_TRACY)
        sky_find_3rd(TARGET tracy         DIR tracy)
        add_definitions(-DTRACY_ENABLE)
    endif ()

    if (SKY_BUILD_BULLET)
        sky_find_3rd(TARGET bullet3       DIR bullet3)
    endif ()

    if (SKY_BUILD_RECAST)
        sky_find_3rd(TARGET recast        DIR recast)
    endif ()

    if (SKY_BUILD_EDITOR)
        sky_find_3rd(TARGET assimp        DIR assimp)
        sky_find_3rd(TARGET meshoptimizer DIR meshoptimizer)
        sky_find_3rd(TARGET stb           DIR stb)
        sky_find_3rd(TARGET ispc_texcomp  DIR ispc_texcomp)
        sky_find_3rd(TARGET GKlib         DIR GKlib)
        sky_find_3rd(TARGET metis         DIR metis)
        sky_find_3rd(TARGET ImGuizmo      DIR ImGuizmo)
    endif ()
elseif (SKY_THIRDPARTY_MODE STREQUAL "PREBUILT")
    message(FATAL_ERROR
        "Third-party mode is PREBUILT but 3RD_PATH is not available.\n"
        "Use one of:\n"
        "  1. Build prebuilt third-party tree: python3 python/third_party.py -p <platform>\n"
        "  2. Configure 3RD_PATH explicitly: cmake -B build -DSKY_THIRDPARTY_MODE=PREBUILT -D3RD_PATH=<path>\n"
        "  3. Switch mode: cmake -B build -DSKY_THIRDPARTY_MODE=LOCAL\n"
    )
else()
    message(FATAL_ERROR "Unsupported SKY_THIRDPARTY_MODE='${SKY_THIRDPARTY_MODE}'")
endif()
