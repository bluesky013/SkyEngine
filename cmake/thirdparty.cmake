set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ENGINE_ROOT}/cmake/thirdparty)
include(${ENGINE_ROOT}/cmake/thirdparty_helpers.cmake)

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

if(EXISTS ${3RD_PATH})
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

    # temp
    sky_find_3rd(TARGET cxxopts       DIR cxxopts)

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
        sky_find_3rd(TARGET PerlinNoise   DIR PerlinNoise)
        sky_find_3rd(TARGET ispc_texcomp  DIR ispc_texcomp)
        sky_find_3rd(TARGET GKlib         DIR GKlib)
        sky_find_3rd(TARGET metis         DIR metis)
        sky_find_3rd(TARGET ImGuizmo      DIR ImGuizmo)
        sky_find_3rd(TARGET nodeeditor    DIR nodeeditor)
    endif ()
else()
    message(FATAL_ERROR "3rdParty folder: ${3RD_PATH} does not exist, call cmake defining a valid 3RD_PATH")
endif()
