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
    if (TARGET Aurora.RHITest)
        target_link_libraries(Aurora.RHITest PRIVATE 3rdParty::dxcompiler)
    endif ()
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
sky_add_module(TARGET Aurora.ShaderCompiler
    SOURCES
        module/AuroraShaderCompilerModule.cpp
    LINK_LIBS
        Aurora.ShaderCompiler.Static
        Framework
)
sky_add_dependency(TARGET Aurora.ShaderCompiler DEPENDENCIES Launcher Editor SandboxEditor AssetTool)

# Offline tools are independent of C++ pass declarations.
if (NOT SKY_MOBILE)
    sky_add_exe(TARGET ShaderHeaderTool
        SOURCES
            tool/ShaderHeaderTool.cpp
        INCS
            include
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        LIBS
            Aurora.Core.Static
            Core
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
endif()

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
    target_include_directories(Aurora.ShaderTest PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../core/test")
    target_compile_definitions(Aurora.ShaderTest PRIVATE
        SHADER_TEST_BUNDLE_ROOT="$<TARGET_FILE_DIR:Aurora.ShaderTest>/test-assets/shaders")

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
