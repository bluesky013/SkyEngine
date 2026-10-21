file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

file(GLOB SHARED_HEADERS shaders/*.h)

sky_add_library(TARGET Aurora.Pipeline.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        ${SHARED_HEADERS}
    PRIVATE_INC
        src
        shaders
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        Aurora.RHI.Static
        Aurora.Core.Static
)

file(GLOB_RECURSE TEST_FILES test/*)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Aurora.PipelineTest
        SOURCES
            ${TEST_FILES}
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test/main.cpp
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test
            ${CMAKE_CURRENT_SOURCE_DIR}/../core/test
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        LIBS
            Aurora.Pipeline.Static
            Aurora.ShaderCompiler.Static
            3rdParty::googletest
    )
    target_compile_definitions(Aurora.PipelineTest PRIVATE
        AURORA_SHADER_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders"
        AURORA_TEST_SHADER_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/assets/shaders"
        SHADER_TEST_SOURCE_ROOT="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/assets/shaders")
endif ()
