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

    # test
    sky_find_3rd(TARGET googletest    DIR googletest)

    if (SKY_USE_TRACY)
        sky_find_3rd(TARGET tracy         DIR tracy)
        add_definitions(-DTRACY_ENABLE)
    endif ()
else()
    message(FATAL_ERROR "3rdParty folder: ${3RD_PATH} does not exist, call cmake defining a valid 3RD_PATH")
endif()
