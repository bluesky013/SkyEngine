set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/thirdparty)

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

set(SKY_3RD_REMOTE "115.159.67.235")
set(SKY_3RD_PACKAGE_NAME Sky3rd-${CMAKE_SYSTEM_NAME}.rar)
set(SKY_3RD_REMOTE_URL ${SKY_3RD_REMOTE}/${SKY_3RD_PACKAGE_NAME})
set(SKY_3RD_SAVE_PATH ${3RD_PATH}/${SKY_3RD_PACKAGE_NAME})

message("[3rdPath]:" ${SKY_3RD_SAVE_PATH})
if (NOT EXISTS ${SKY_3RD_SAVE_PATH})
    file(DOWNLOAD
        ${SKY_3RD_REMOTE_URL} ${SKY_3RD_SAVE_PATH}
        SHOW_PROGRESS
    )
    file(ARCHIVE_EXTRACT
        INPUT ${SKY_3RD_SAVE_PATH}
        DESTINATION ${3RD_PATH}
    )
endif()

if(EXISTS ${3RD_PATH})
    # core
    sky_find_3rd(TARGET crc32         DIR crc32c)
    sky_find_3rd(TARGET sfmt          DIR sfmt)
    sky_find_3rd(TARGET boost         DIR boost)
    sky_find_3rd(TARGET taskflow      DIR taskflow)

    # framework
    sky_find_3rd(TARGET rapidjson     DIR rapidjson)
    sky_find_3rd(TARGET sdl           DIR sdl)
    sky_find_3rd(TARGET sqlite        DIR sqlite)

    # vulkan
    sky_find_3rd(TARGET volk          DIR volk)
    sky_find_3rd(TARGET vma           DIR VulkanMemoryAllocator)

    # tmp
    sky_find_3rd(TARGET imgui         DIR imgui)

    # shader
    sky_find_3rd(TARGET glslang       DIR glslang)
    sky_find_3rd(TARGET SPIRVCross    DIR SPIRV-Cross)

    # test
    sky_find_3rd(TARGET googletest    DIR googletest)

    if (SKY_BUILD_PERF_TOOL)
        sky_find_3rd(TARGET implot        DIR implot)
    endif()

    if (SKY_BUILD_TOOL)
        sky_find_3rd(TARGET assimp        DIR assimp)
        sky_find_3rd(TARGET meshoptimizer DIR meshoptimizer)
        sky_find_3rd(TARGET stb           DIR stb)
        sky_find_3rd(TARGET cxxopts       DIR cxxopts)
        sky_find_3rd(TARGET PerlinNoise   DIR PerlinNoise)
        sky_find_3rd(TARGET ktx           DIR ktx)
        sky_find_3rd(TARGET ispc_texcomp  DIR ispc_texcomp)
    endif ()

    if (WIN32 OR ANDROID)
        sky_find_3rd(TARGET gles          DIR gles)
    endif()
else()
    message(FATAL_ERROR "3rdParty folder: ${3RD_PATH} does not exist, call cmake defining a valid 3RD_PATH")
endif()
