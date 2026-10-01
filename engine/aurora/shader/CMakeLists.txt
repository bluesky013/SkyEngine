file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_find_3rd(TARGET slang DIR slang)

set(AuroraShaderExtLibs 3rdParty::slang)
if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    # slang's DXIL backend loads dxcompiler.dll / dxil.dll at runtime; declare
    # the dependency so the DLLs are copied next to the shader compiler module.
    if (NOT TARGET 3rdParty::dxcompiler)
        sky_find_3rd(TARGET dxcompiler DIR dxcompiler)
    endif ()
    list(APPEND AuroraShaderExtLibs 3rdParty::dxcompiler)
endif ()

# Slang compiler implementation. Depends on aurora/core (ShaderRef / variants /
# ShaderFileSystem / RgBlockDesc) which stays slang-free.
sky_add_library(TARGET Aurora.ShaderCompiler.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        Aurora.Core.Static
        Aurora.RHI.Static
        ${AuroraShaderExtLibs}
)

# Optional framework module: registers the compiler into ShaderCompilerFactory.
# Apps that omit it fall back to cache-only shader resolution.
sky_add_library(TARGET Aurora.ShaderCompiler SHARED
    SOURCES
        module/AuroraShaderCompilerModule.cpp
    LINK_LIBS
        Aurora.ShaderCompiler.Static
        Framework
)
sky_add_dependency(TARGET Aurora.ShaderCompiler DEPENDENCIES Launcher Editor)

# ---- offline shader header codegen (host tool) ----

sky_add_exe(TARGET ShaderHeaderTool
    SOURCES
        tool/ShaderHeaderTool.cpp
    INCS
        include
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    LIBS
        Aurora.ShaderCompiler.Static
        Core
        3rdParty::slang
)

# offline shader cook: usage.json -> index/blobs
sky_add_exe(TARGET ShaderCookTool
    SOURCES
        tool/ShaderCookTool.cpp
    INCS
        include
    LIBS
        Aurora.ShaderCompiler.Static
        Core
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
    sky_add_test(TARGET Aurora.ShaderTest
        SOURCES
            ${TEST_FILES}
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test
        LIBS
            Aurora.ShaderCompiler.Static
            Aurora.RHI.Static
            3rdParty::googletest
    )

    # backends are dlopen'd at runtime; only need them built, not linked
    if (TARGET Aurora.Vulkan)
        add_dependencies(Aurora.ShaderTest Aurora.Vulkan)
    endif()
    if (TARGET Aurora.DX12)
        add_dependencies(Aurora.ShaderTest Aurora.DX12)
    endif()
    if (TARGET Aurora.Metal)
        add_dependencies(Aurora.ShaderTest Aurora.Metal)
    endif()
endif ()
