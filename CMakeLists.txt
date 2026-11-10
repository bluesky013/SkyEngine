cmake_minimum_required(VERSION 3.19)  # 3.19+ required for string(JSON ...)

PROJECT(SkyEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(ENGINE_ROOT ${CMAKE_CURRENT_LIST_DIR})

include(${ENGINE_ROOT}/cmake/functions.cmake)
include(${ENGINE_ROOT}/cmake/options.cmake)
include(${ENGINE_ROOT}/cmake/plugin_switches.cmake)
include(${ENGINE_ROOT}/cmake/configuration.cmake)
include(${ENGINE_ROOT}/cmake/thirdparty.cmake)

if (CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    if (NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    endif ()
    if (NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    endif ()
elseif (NOT ANDROID)
    if (NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ENGINE_ROOT}/output/bin)
    endif()
    if (NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ENGINE_ROOT}/output/bin)
    endif()
    if (NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEVELOP)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEVELOP ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Develop)
    endif()
    if (NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEVELOP)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEVELOP ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Develop)
    endif()
endif()

if (SKY_BUILD_TEST)
    enable_testing()
endif()

add_subdirectory(engine)
add_subdirectory(plugins)

get_property(module_resource_configs GLOBAL PROPERTY SKY_MODULE_RESOURCE_CONFIGS)
sky_runtime_resource_directory(runtime_resource_directory)
set(shader_resource_directory "${runtime_resource_directory}/assets/shaders")
sky_stage_shader_resources(TARGET ShaderResources.Stage
    DESTINATION "${shader_resource_directory}"
    CONFIGS ${module_resource_configs}
    CONSUMERS SandboxEditor Aurora.Render Aurora.PipelineTest Aurora.Renderer.GpuSubmissionTest Particle.Render)
install(DIRECTORY "${shader_resource_directory}/" DESTINATION assets/shaders)
if(SKY_BUILD_TEST)
    sky_stage_shader_resources(TARGET ShaderResources.TestFixtures
        DESTINATION "${runtime_resource_directory}/test-assets/shaders"
        CONFIGS "${ENGINE_ROOT}/engine/aurora/resources.json"
            "${ENGINE_ROOT}/engine/aurora/core/test/plugins/first/plugin.json"
            "${ENGINE_ROOT}/engine/aurora/core/test/plugins/second/plugin.json"
            "${ENGINE_ROOT}/engine/aurora/pipeline/test/resources.json"
        CONSUMERS Aurora.ShaderTest Aurora.PipelineTest)
endif()

# Runtime selections also determine targeted module build dependencies.
if (TARGET SandboxEditor)
    sky_resolve_module_manifest(_sandbox_modules "${ENGINE_ROOT}/configs/modules_editor.json")
    add_dependencies(SandboxEditor ${_sandbox_modules})
endif ()

# Tools are always available on desktop (AssetTool is the out-of-process cook worker);
# the SKY_BUILD_TOOL gate only controls the optional developer tools inside tools/.
if (NOT ANDROID AND NOT CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    add_subdirectory(tools)
endif()


# The runtime host (`Launcher`) is project-owned (see the `add-project-native-layout` change).
if (SKY_BUILD_TEST)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME CMakeSourceOwnership
        COMMAND ${CMAKE_COMMAND} -E env "CMAKE_GENERATOR=${CMAKE_GENERATOR}"
            ${Python3_EXECUTABLE} "${ENGINE_ROOT}/python/tests/test_cmake_source_ownership.py")
    set_tests_properties(CMakeSourceOwnership PROPERTIES TIMEOUT 180)
endif ()
