file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

file(GLOB SHARED_HEADERS shaders/*.h)

set(SCENE_SHADER_PATHS
    aurora/shader/SharedShader.h
    ScenePass.h
    SceneBatch.h
    SceneForward.slang
    SceneLightingPass.h
    SceneSurfaceBatch.h
    ShadowPass.h
    SceneShadow.slang
    SceneLit.slang
    ToneMapPass.h
    SceneToneMap.slang
    SceneEnvironmentPass.h
)
set(SCENE_SHADER_SOURCES "")
foreach (SHADER_PATH IN LISTS SCENE_SHADER_PATHS)
    if (SHADER_PATH STREQUAL "aurora/shader/SharedShader.h")
        set(shader_file "${PROJECT_SOURCE_DIR}/engine/aurora/core/include/${SHADER_PATH}")
    else ()
        set(shader_file "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER_PATH}")
    endif ()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${shader_file}")
    file(READ "${shader_file}" SHADER_SOURCE)
    string(APPEND SCENE_SHADER_SOURCES "        {\"${SHADER_PATH}\", R\"AURORA_SHADER(${SHADER_SOURCE})AURORA_SHADER\"},\n")
endforeach ()
configure_file(shaders/ShaderSource.h.in "generated/SceneShaderSources.h" @ONLY)

sky_add_library(TARGET Aurora.Pipeline.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        ${SHARED_HEADERS}
    PRIVATE_INC
        src
        shaders
        ${CMAKE_CURRENT_BINARY_DIR}/generated
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
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        LIBS
            Aurora.Pipeline.Static
            Aurora.ShaderCompiler.Static
            3rdParty::googletest
    )
    target_compile_definitions(Aurora.PipelineTest PRIVATE
        AURORA_SHARED_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        AURORA_SHADER_INCLUDE_DIR="${PROJECT_SOURCE_DIR}/engine/aurora/core/include")
endif ()
