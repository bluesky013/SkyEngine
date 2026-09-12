file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_find_3rd(TARGET slang DIR slang)

sky_add_library(TARGET Aurora.Shader STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        Aurora.RHI
        3rdParty::slang
)

# ---- offline shader header codegen (host tool) ----

sky_add_exe(TARGET ShaderHeaderTool
    SOURCES
        tool/ShaderHeaderTool.cpp
    INCS
        include
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    LIBS
        Aurora.Shader
        Core
        3rdParty::slang
)

# slang shader assets, organized by resource tier:
#   assets/shaders/slang/global/    -> set 0 (GlobalRenderResources)
#   assets/shaders/slang/pipeline/  -> set 1 (PipelinePass)
#   assets/shaders/slang/batch/     -> set 2 (per-draw dynamic UBO)
set(SLANG_SHADER_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/slang)

set(SHADER_GEN_DIR ${CMAKE_BINARY_DIR}/Gen/ShaderHeaders)
file(MAKE_DIRECTORY ${SHADER_GEN_DIR})

set(GLOBAL_BLOCK_SRC ${SLANG_SHADER_ROOT}/global/GlobalBlock.slang)
set(GLOBAL_BLOCK_GEN ${SHADER_GEN_DIR}/GlobalBlock.gen.h)

add_custom_command(
    OUTPUT ${GLOBAL_BLOCK_GEN}
    COMMAND $<TARGET_FILE:ShaderHeaderTool> ${GLOBAL_BLOCK_SRC} ${GLOBAL_BLOCK_GEN}
    DEPENDS ShaderHeaderTool ${GLOBAL_BLOCK_SRC}
    VERBATIM
)

add_custom_target(ShaderHeaders DEPENDS ${GLOBAL_BLOCK_GEN})

if (SKY_BUILD_TEST)
    sky_add_test(TARGET AuroraShaderTest
        SOURCES
            ${TEST_FILES}
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test
        LIBS
            Aurora.Shader
            Aurora.RHI
            3rdParty::googletest
    )

    # backends are dlopen'd at runtime; only need them built, not linked
    if (TARGET AuroraVulkan)
        add_dependencies(AuroraShaderTest AuroraVulkan)
    endif()
    if (TARGET AuroraDX12)
        add_dependencies(AuroraShaderTest AuroraDX12)
    endif()
    if (TARGET AuroraMetal)
        add_dependencies(AuroraShaderTest AuroraMetal)
    endif()
endif ()
