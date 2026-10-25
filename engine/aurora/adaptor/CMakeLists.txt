file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

# Static aurora -> framework bridge:
# reflection + assets + components + the IModule implementation (AuroraModule).
# The launcher/editor module entry points live in engine/aurora/{runtime,editor}.
sky_add_library(TARGET Aurora.Adaptor.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PUBLIC_INC
        include
    PRIVATE_INC
        ${CMAKE_CURRENT_SOURCE_DIR}/../pipeline/shaders
    LINK_LIBS
        Aurora.Core.Static
        Aurora.Pipeline.Static
        Aurora.RHI.Static
        Animation
        Framework
        Core
)

file(GLOB_RECURSE TEST_FILES test/*)
if (SKY_BUILD_TEST)
    sky_add_test(TARGET Aurora.AdaptorTest
        SOURCES
            ${TEST_FILES}
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test
            ${CMAKE_CURRENT_SOURCE_DIR}/../core/test
        LIBS
            Aurora.Adaptor.Static
            Aurora.RHI.Static
            Aurora.ShaderCompiler.Static
            Core
            3rdParty::googletest
    )
    target_compile_definitions(Aurora.AdaptorTest PRIVATE
        SHADER_TEST_SOURCE_ROOT="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/assets/shaders")
endif ()
