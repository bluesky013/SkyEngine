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

include(ExternalProject)
function(sky_add_external_3rd)
    cmake_parse_arguments(TMP
        ""
        "TARGET"
        "DIR;COMPILE_OPTIONS"
        ${ARGN}
        )

    ExternalProject_Add(
        ${TMP_TARGET}
        CMAKE_ARGS
        ${TMP_COMPILE_OPTIONS} -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/install/${TMP_DIR}
        -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG=${CMAKE_BINARY_DIR}/install/${TMP_DIR}/lib/Debug
        -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=${CMAKE_BINARY_DIR}/install/${TMP_DIR}/lib/Release
        -DCMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG=${CMAKE_BINARY_DIR}/install/${TMP_DIR}/lib/Debug
        -DCMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE=${CMAKE_BINARY_DIR}/install/${TMP_DIR}/lib/Release
        -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=${CMAKE_BINARY_DIR}/install/${TMP_DIR}/bin/Debug
        -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=${CMAKE_BINARY_DIR}/install/${TMP_DIR}/bin/Release
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${TMP_DIR}
        BINARY_DIR ${CMAKE_BINARY_DIR}/${TMP_DIR}
    )
endfunction()

sky_add_external_3rd(TARGET vma
    DIR VulkanMemoryAllocator)

sky_add_external_3rd(TARGET glm
    DIR glm
    COMPILE_OPTIONS
    -DBUILD_SHARED_LIBS=OFF
    -DBUILD_STATIC_LIBS=OFF
    -DBUILD_TESTING=OFF)

sky_add_external_3rd(TARGET rapidjson
    DIR rapidjson
    COMPILE_OPTIONS
    -DCMAKE_INSTALL_DIR=${CMAKE_BINARY_DIR}/install/rapidjson
    -DDOC_INSTALL_DIR=${CMAKE_BINARY_DIR}/install/rapidjson/docs
    -DINCLUDE_INSTALL_DIR=${CMAKE_BINARY_DIR}/install/rapidjson/include
    -DLIB_INSTALL_DIR=${CMAKE_BINARY_DIR}/install/rapidjson/lib
    -DRAPIDJSON_BUILD_DOC=OFF
    -DRAPIDJSON_BUILD_EXAMPLES=OFF
    -DRAPIDJSON_BUILD_TESTS=OFF
    -DRAPIDJSON_BUILD_CXX11=OFF
    -DRAPIDJSON_BUILD_CXX17=ON
    -DRAPIDJSON_ENABLE_INSTRUMENTATION_OPT=OFF
    -DBUILD_TESTING=OFF)

sky_add_external_3rd(TARGET taskflow
    DIR taskflow
    COMPILE_OPTIONS
    -DTF_BUILD_TESTS=OFF
    -DTF_BUILD_EXAMPLES=OFF
    -DBUILD_TESTING=OFF)

sky_add_external_3rd(TARGET crc32c
    DIR crc32c
    COMPILE_OPTIONS
    -DCRC32C_BUILD_BENCHMARKS=OFF
    -DCRC32C_BUILD_TESTS=OFF
    -DCRC32C_INSTALL=ON
    -DCRC32C_USE_GLOG=OFF
    -DBUILD_SHARED_LIBS=OFF)

sky_add_external_3rd(TARGET cereal
    DIR cereal
    COMPILE_OPTIONS
    -DJUST_INSTALL_CEREAL=ON)

sky_add_external_3rd(TARGET assimp
    DIR assimp
    COMPILE_OPTIONS
    -DASSIMP_BUILD_TESTS=OFF
    -DBUILD_SHARED_LIBS=OFF)

#sky_add_external_3rd(TARGET shaderc
#    DIR shaderc
#    COMPILE_OPTIONS
#    -DBUILD_TESTING=OFF
#    -DENABLE_CTEST=OFF
#    -DSHADERC_SKIP_TESTS=ON
#    -DSHADERC_GLSLANG_DIR=${CMAKE_CURRENT_SOURCE_DIR}/glslang
#    -DSHADERC_SPIRV_TOOLS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Tools
#    -DSHADERC_SPIRV_HEADERS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Headers)

sky_add_external_3rd(TARGET SpvCross
    DIR SPIRV-Cross
    COMPILE_OPTIONS
    -SPIRV_CROSS_CLI=OFF
    -SPIRV_CROSS_ENABLE_TESTS=OFF)

sky_add_external_3rd(TARGET cxxopts
    DIR cxxopts
    COMPILE_OPTIONS
    -DCXXOPTS_BUILD_EXAMPLES=OFF
    -DCXXOPTS_BUILD_TESTS=OFF)

sky_add_external_3rd(TARGET sdl
    DIR sdl
    COMPILE_OPTIONS
    -DSDL_TEST=OFF
    -DSDL_SHARED=OFF)

sky_add_external_3rd(TARGET freetype
    DIR freetype
    COMPILE_OPTIONS)