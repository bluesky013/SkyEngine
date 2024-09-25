file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
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

sky_add_dependency(TARGET ShaderCompiler DEPENDENCIES Launcher AssetBuilder Editor)
