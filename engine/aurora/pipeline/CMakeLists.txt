file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET Aurora.Pipeline.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
        ${CMAKE_BINARY_DIR}/Gen/ShaderHeaders
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        Aurora.RHI.Static
        Aurora.Core.Static
)

# generated shader headers must exist before Aurora.Pipeline.Static compiles
add_dependencies(Aurora.Pipeline.Static ShaderHeaders)

file(GLOB_RECURSE TEST_FILES test/*)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Aurora.PipelineTest
        SOURCES
            ${TEST_FILES}
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test/main.cpp
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test
        LIBS
            Aurora.Pipeline.Static
            Aurora.ShaderCompiler.Static
            3rdParty::googletest
    )
endif ()
