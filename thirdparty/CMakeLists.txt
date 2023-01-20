cmake_minimum_required(VERSION 3.10.0)
PROJECT(SkyEngineThirdparty)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)

function(sky_install_3rd)
    cmake_parse_arguments(TMP
        ""
        "TARGET"
        ""
        ${ARGN}
        )

    install(TARGETS ${TMP_TARGET}
        CONFIGURATIONS Debug
        EXPORT ${TMP_TARGET}
        LIBRARY DESTINATION ${TMP_TARGET}/lib/Debug
        ARCHIVE DESTINATION ${TMP_TARGET}/lib/Debug
        RUNTIME DESTINATION ${TMP_TARGET}/bin/Debug
        PUBLIC_HEADER DESTINATION ${TMP_TARGET}/include
        )

    install(TARGETS ${TMP_TARGET}
        CONFIGURATIONS Release
        EXPORT ${TMP_TARGET}
        LIBRARY DESTINATION ${TMP_TARGET}/lib/Release
        ARCHIVE DESTINATION ${TMP_TARGET}/lib/Release
        RUNTIME DESTINATION ${TMP_TARGET}/bin/Release
        PUBLIC_HEADER DESTINATION ${TMP_TARGET}/include
        )

endfunction()

function(sky_add_custom_3rd)
    cmake_parse_arguments(TMP
        "STATIC;SHARED;INTERFACE"
        "TARGET"
        "SOURCES;PUBLIC_HEADERS;PUBLIC_INCLUDES;PRIVATE_INCLUDES"
        ${ARGN}
        )

    unset(TMP_TYPE)
    if (TMP_STATIC)
        set(TMP_TYPE STATIC)
    elseif(TMP_SHARED)
        set(TMP_TYPE SHARED)
    elseif(TMP_INTERFACE)
        set(TMP_TYPE INTERFACE)
    endif()

    add_library(${TMP_TARGET} ${TMP_TYPE}
        ${TMP_SOURCES}
        )
    if(TMP_PUBLIC_HEADERS)
        set_target_properties(${TMP_TARGET} PROPERTIES PUBLIC_HEADER "${TMP_PUBLIC_HEADERS}")
    endif()

    if (TMP_PUBLIC_INCLUDES)
        target_include_directories(${TMP_TARGET} PUBLIC ${TMP_PUBLIC_INCLUDES})
        install(DIRECTORY "${TMP_PUBLIC_INCLUDES}" DESTINATION "${CMAKE_INSTALL_PREFIX}/${TMP_TARGET}")
    endif()

    if (TMP_PRIVATE_INCLUDES)
        target_include_directories(${TMP_TARGET} PRIVATE ${TMP_PRIVATE_INCLUDES})
    endif()

    sky_install_3rd(TARGET ${TMP_TARGET})
endfunction()

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(IMGUI_SRC
    ${IMGUI_DIR}/imgui.h
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    )

sky_add_custom_3rd(TARGET Imgui STATIC
    SOURCES ${IMGUI_SRC}
    PUBLIC_HEADERS
        ${IMGUI_DIR}/imgui.h
        ${IMGUI_DIR}/imconfig.h
)

set(STB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/stb)
set(STB_SRC
    ${STB_DIR}/stb_image.h
    ${STB_DIR}/stb_image_write.h
    )
sky_add_custom_3rd(TARGET stb INTERFACE
    SOURCES ${STB_SRC}
    PUBLIC_HEADERS ${STB_SRC})

set(SFMT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SFMT-src-1.5.1)
set(SFMT_HEADERS
    ${SFMT_DIR}/SFMT.h
    ${SFMT_DIR}/SFMT-params.h
    ${SFMT_DIR}/SFMT-params19937.h
    )
sky_add_custom_3rd(TARGET Sfmt STATIC
    SOURCES ${SFMT_DIR}/SFMT.c
    PUBLIC_HEADERS ${SFMT_HEADERS}
    )
target_compile_definitions(Sfmt PUBLIC -DSFMT_MEXP=19937)

set(GTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/googletest)
sky_add_custom_3rd(TARGET googletest STATIC
    SOURCES ${GTEST_DIR}/googletest/src/gtest-all.cc
    PUBLIC_INCLUDES ${GTEST_DIR}/googletest/include
    PRIVATE_INCLUDES ${GTEST_DIR}/googletest)
