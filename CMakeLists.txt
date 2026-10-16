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
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ENGINE_ROOT}/output/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ENGINE_ROOT}/output/bin)
endif()

if (SKY_BUILD_TEST)
    enable_testing()
endif()

add_subdirectory(engine)
add_subdirectory(plugins)

# Runtime modules are loaded by name, so linking the host does not rebuild them.
# Bind after plugins are configured, including only modules enabled in this build.
if (TARGET SandboxEditor)
    set(_sandbox_module_config "${ENGINE_ROOT}/configs/modules_editor.json")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_sandbox_module_config}")
    file(READ "${_sandbox_module_config}" _sandbox_module_json)
    string(JSON _sandbox_module_count LENGTH "${_sandbox_module_json}" modules)
    if (_sandbox_module_count GREATER 0)
        math(EXPR _sandbox_module_last "${_sandbox_module_count} - 1")
        foreach (_sandbox_module_index RANGE ${_sandbox_module_last})
            string(JSON _sandbox_module_name GET "${_sandbox_module_json}" modules ${_sandbox_module_index} name)
            if (TARGET "${_sandbox_module_name}")
                add_dependencies(SandboxEditor "${_sandbox_module_name}")
            endif ()
        endforeach ()
    endif ()
    unset(_sandbox_module_config)
    unset(_sandbox_module_json)
    unset(_sandbox_module_count)
    unset(_sandbox_module_last)
    unset(_sandbox_module_index)
    unset(_sandbox_module_name)
endif ()

# Tools are always available on desktop (AssetTool is the out-of-process cook worker);
# the SKY_BUILD_TOOL gate only controls the optional developer tools inside tools/.
if (NOT ANDROID)
    add_subdirectory(tools)
endif()


# The runtime host (`Launcher`) is project-owned (see the `add-project-native-layout` change).
if (SKY_BUILD_EDITOR)
    sky_set_dependency(TARGET Editor)
endif()
