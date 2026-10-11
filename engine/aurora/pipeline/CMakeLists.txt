file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET Aurora.Pipeline.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
        ${CMAKE_CURRENT_SOURCE_DIR}/generated
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        Aurora.RHI.Static
        Aurora.Core.Static
)

# `generated/GlobalBlock.gen.h` is pre-generated on a host machine from
# GlobalBlock.slang and committed, so cross builds (iOS/Android) do not need to
# run the ShaderHeaderTool host tool. When the tool target exists (desktop),
# ensure it regenerates the header before this compiles.
if (TARGET ShaderHeaders)
    add_dependencies(Aurora.Pipeline.Static ShaderHeaders)
endif ()

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
