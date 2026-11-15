file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

set(ASSET_SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshAsset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SkinAsset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MaterialReflection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MaterialDefinition.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/assets/LodGroupAsset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/assets/AuroraAssetReflection.cpp
)
list(REMOVE_ITEM SRC_FILES ${ASSET_SRC_FILES})

sky_add_library(TARGET Aurora.Assets.Static STATIC
    SOURCES
        ${ASSET_SRC_FILES}
    PUBLIC_INC
        include
    LINK_LIBS
        Aurora.Core.Static
        Animation
        Framework
        Core
)

# Runtime/world integration consumes the shared persistent formats.
sky_add_library(TARGET Aurora.Adaptor.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PUBLIC_INC
        include
    PRIVATE_INC
        ${CMAKE_CURRENT_SOURCE_DIR}/../pipeline/shaders
    LINK_LIBS
        Aurora.Assets.Static
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
            ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer
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
