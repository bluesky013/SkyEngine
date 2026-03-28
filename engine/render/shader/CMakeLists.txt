file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)
file(GLOB_RECURSE TOOL_FILES tool/*)
file(GLOB_RECURSE TEST_FILES test/*)

# shader
sky_find_3rd(TARGET glslang       DIR glslang)
sky_find_3rd(TARGET SPIRVCross    DIR SPIRV-Cross)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    sky_find_3rd(TARGET dxcompiler    DIR dxcompiler)

    set(ShaderCompilerExtLib
            3rdParty::glslang
            3rdParty::SPIRVCross
            3rdParty::dxcompiler
    )
else ()
    set(ShaderCompilerExtLib
            3rdParty::glslang
            3rdParty::SPIRVCross
    )
endif ()

sky_add_library(TARGET ShaderCompiler.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        RHI
        ${ShaderCompilerExtLib}
    INSTALL_DIR
        include
    )

sky_add_library(TARGET ShaderCompiler SHARED
    SOURCES
        ShaderModule.cpp
    PRIVATE_INC
    PUBLIC_INC
    LINK_LIBS
        Framework
        ShaderCompiler.Static
)

sky_add_exe(TARGET ShaderTool
    SOURCES
        ${TOOL_FILES}
    INCS
        tool
    LIBS
        Framework
        ShaderCompiler.Static
)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET ShaderCompilerTest
        SOURCES
            ${TEST_FILES}
        LIBS
            ShaderCompiler.Static
            3rdParty::googletest
    )
endif ()

add_dependencies(ShaderTool ShaderCompiler)
sky_add_dependency(TARGET ShaderCompiler DEPENDENCIES Launcher AssetBuilder Editor)
