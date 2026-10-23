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

if (NOT ANDROID)
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
    CONSUMERS SandboxEditor Aurora.Render Aurora.PipelineTest Aurora.Renderer.GpuSubmissionTest)
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

# Runtime module configuration also orders targeted module builds.
# Bind after plugins are configured, including only enabled targets.
if (TARGET SandboxEditor)
    set(_sandbox_module_config "${ENGINE_ROOT}/configs/modules_editor.json")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_sandbox_module_config}")
    file(READ "${_sandbox_module_config}" _sandbox_module_json)
    string(JSON _sandbox_module_count LENGTH "${_sandbox_module_json}" modules)
    set(_sandbox_module_targets SandboxEditor)
    if (_sandbox_module_count GREATER 0)
        math(EXPR _sandbox_module_last "${_sandbox_module_count} - 1")
        foreach (_sandbox_module_index RANGE ${_sandbox_module_last})
            string(JSON _sandbox_module_name GET "${_sandbox_module_json}" modules ${_sandbox_module_index} name)
            if (TARGET "${_sandbox_module_name}")
                sky_add_dependency(TARGET "${_sandbox_module_name}" DEPENDENCIES SandboxEditor)
                list(APPEND _sandbox_module_targets "${_sandbox_module_name}")
                string(JSON _sandbox_dependency_count LENGTH "${_sandbox_module_json}" modules ${_sandbox_module_index} dependencies)
                if (_sandbox_dependency_count GREATER 0)
                    math(EXPR _sandbox_dependency_last "${_sandbox_dependency_count} - 1")
                    foreach (_sandbox_dependency_index RANGE ${_sandbox_dependency_last})
                        string(JSON _sandbox_dependency_name GET "${_sandbox_module_json}" modules ${_sandbox_module_index} dependencies ${_sandbox_dependency_index})
                        if (TARGET "${_sandbox_dependency_name}")
                            sky_add_dependency(TARGET "${_sandbox_dependency_name}" DEPENDENCIES "${_sandbox_module_name}")
                        endif ()
                    endforeach ()
                endif ()
            endif ()
        endforeach ()
    endif ()
    foreach (_sandbox_module_target IN LISTS _sandbox_module_targets)
        sky_set_dependency(TARGET "${_sandbox_module_target}")
    endforeach ()
    unset(_sandbox_module_config)
    unset(_sandbox_module_json)
    unset(_sandbox_module_count)
    unset(_sandbox_module_last)
    unset(_sandbox_module_index)
    unset(_sandbox_module_name)
    unset(_sandbox_module_targets)
    unset(_sandbox_module_target)
    unset(_sandbox_dependency_count)
    unset(_sandbox_dependency_last)
    unset(_sandbox_dependency_index)
    unset(_sandbox_dependency_name)
endif ()

# Tools are always available on desktop (AssetTool is the out-of-process cook worker);
# the SKY_BUILD_TOOL gate only controls the optional developer tools inside tools/.
if (NOT ANDROID)
    add_subdirectory(tools)
endif()


# The runtime host (`Launcher`) is project-owned (see the `add-project-native-layout` change).
