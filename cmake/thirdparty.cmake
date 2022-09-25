SET(3RD_PATH "" CACHE STRING "SkyEngine 3rd path")

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

if(EXISTS ${3RD_PATH})
    sky_find_3rd(TARGET rapidjson     DIR rapidjson)
    sky_find_3rd(TARGET SPIRVCross    DIR SPIRV-Cross)
    sky_find_3rd(TARGET vma           DIR VulkanMemoryAllocator)
    sky_find_3rd(TARGET taskflow      DIR taskflow)
    sky_find_3rd(TARGET assimp        DIR assimp)
    sky_find_3rd(TARGET googletest    DIR googletest)
    sky_find_3rd(TARGET crc32         DIR crc32c)
    sky_find_3rd(TARGET imgui         DIR imgui)
    sky_find_3rd(TARGET sfmt          DIR sfmt)
    sky_find_3rd(TARGET shaderc       DIR shaderc)
    sky_find_3rd(TARGET cereal        DIR cereal)
    sky_find_3rd(TARGET stb           DIR stb)
    sky_find_3rd(TARGET cxxopts       DIR cxxopts)
    if (WIN32)
        sky_find_3rd(TARGET sdl           DIR sdl)
    else()
    endif()
else()
    message(FATAL_ERROR "3rdParty folder: ${3RD_PATH} does not exist, call cmake defining a valid 3RD_PATH")
endif()
