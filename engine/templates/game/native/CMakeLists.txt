# {{PROJECT_NAME}} game project (SkyEngine).
#
# Independent CMake entry: consumes the engine as a library (engine + plugin targets)
# instead of adding the engine's root project. Point SKY_ENGINE_ROOT at the SkyEngine
# repository (via a preset or the environment).
cmake_minimum_required(VERSION 3.20)

project({{PROJECT_NAME}} LANGUAGES C CXX)

if (APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

    # Packaged modules/backends resolve each other inside the app, independently
    # of the machine or build directory that produced them.
    set(CMAKE_MACOSX_RPATH ON)
    set(CMAKE_INSTALL_NAME_DIR "@rpath")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
    set(CMAKE_INSTALL_RPATH "@loader_path")
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_C_STANDARD 17)

if (NOT DEFINED SKY_ENGINE_ROOT OR SKY_ENGINE_ROOT STREQUAL "")
    set(SKY_ENGINE_ROOT "$ENV{SKY_ENGINE_ROOT}")
endif ()
if (SKY_ENGINE_ROOT STREQUAL "")
    message(FATAL_ERROR "SKY_ENGINE_ROOT is not set: point it at the SkyEngine repository.")
endif ()
set(ENGINE_ROOT "${SKY_ENGINE_ROOT}")

# Platform name used for the output tree layout (<Project>/build/<platform>/bin).
if (NOT DEFINED SKY_PLATFORM OR SKY_PLATFORM STREQUAL "")
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" SKY_PLATFORM)
    if (SKY_PLATFORM STREQUAL "darwin")
        set(SKY_PLATFORM "macos")
    endif ()
endif ()

# Unified runtime/library output. Android must NOT override these (gradle's
# externalNativeBuild collects the shared libraries from the CMake build tree).
if (NOT ANDROID)
    set(_sky_bin "${CMAKE_CURRENT_SOURCE_DIR}/../build/${SKY_PLATFORM}/bin")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${_sky_bin}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${_sky_bin}")
    # Keep the $<CONFIG> subdir consistent with the engine's deploy path
    # (cmake/functions.cmake copies to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>).
    foreach (_cfg Debug Develop Release RelWithDebInfo MinSizeRel)
        string(TOUPPER "${_cfg}" _cfgU)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfgU} "${_sky_bin}/${_cfg}")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_cfgU} "${_sky_bin}/${_cfg}")
    endforeach ()
endif ()

include(${ENGINE_ROOT}/cmake/functions.cmake)
include(${ENGINE_ROOT}/cmake/options.cmake)
include(${ENGINE_ROOT}/cmake/plugin_switches.cmake)
include(${ENGINE_ROOT}/cmake/configuration.cmake)
include(${ENGINE_ROOT}/cmake/thirdparty.cmake)

# Third-party: reuse the engine bootstrap output (python/third_party.py -p <platform>).

add_subdirectory("${ENGINE_ROOT}/engine"  "${CMAKE_BINARY_DIR}/engine")
add_subdirectory("${ENGINE_ROOT}/plugins" "${CMAKE_BINARY_DIR}/plugins")

# Preset-provided extra compile definitions/flags (see export_presets.json).
if (SKY_EXTRA_CXX_DEFINITIONS)
    add_compile_definitions(${SKY_EXTRA_CXX_DEFINITIONS})
endif ()
if (SKY_EXTRA_CXX_FLAGS)
    add_compile_options(${SKY_EXTRA_CXX_FLAGS})
endif ()

# Project-owned platform launcher (defines the `Launcher` target).
if (ANDROID)
    add_subdirectory(android/cpp)
elseif (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_subdirectory(ios)
elseif (APPLE)
    add_subdirectory(macos)
elseif (WIN32)
    add_subdirectory(windows)
endif ()

# Game module(s).
add_subdirectory(source)

# Bind every `DEPENDENCIES Launcher` registration (engine, plugins) to this launcher.
sky_set_dependency(TARGET Launcher)

if(SKY_DEVELOP)
    sky_stage_shader_resources(TARGET ShaderResources.Stage
        DESTINATION "$<TARGET_FILE_DIR:Launcher>/assets/shaders"
        PROJECT "${CMAKE_CURRENT_SOURCE_DIR}/.." ENGINE "${ENGINE_ROOT}"
        CONSUMERS Launcher)
endif()

# Static module packaging: generate the registration cpp from the statically built
# modules and link them into the launcher (their code is compiled in, not dlopen'd).
if (SKY_MODULE_PACKAGING STREQUAL "static")
    set(_sky_static_reg "${CMAKE_CURRENT_BINARY_DIR}/RegisterStaticModules.cpp")
    sky_generate_static_modules("${_sky_static_reg}")
    target_sources(Launcher PRIVATE "${_sky_static_reg}")
    get_property(_sky_static_targets GLOBAL PROPERTY SKY_STATIC_MODULE_TARGETS)
    if (_sky_static_targets)
        target_link_libraries(Launcher ${_sky_static_targets})
    endif ()
endif ()

# Deploy the engine builtin configs first, then the project configs (project wins).
# A custom target is used because `Launcher` is defined in a subdirectory.
add_custom_target({{PROJECT_NAME}}Deploy ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ENGINE_ROOT}/configs"
        "$<TARGET_FILE_DIR:Launcher>/configs"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../configs"
        "$<TARGET_FILE_DIR:Launcher>/configs"
    DEPENDS Launcher
    COMMENT "Deploying engine + project configs"
)

function(_sky_collect_imported_runtimes directory result)
    get_property(_imported DIRECTORY "${directory}" PROPERTY IMPORTED_TARGETS)
    set(_runtime_files "")
    set(_linked_files "")
    foreach (_target IN LISTS _imported)
        # Directory inventories can include non-global imports invisible here.
        if (NOT TARGET "${_target}")
            continue()
        endif ()
        get_target_property(_type "${_target}" TYPE)
        if (_type STREQUAL "SHARED_LIBRARY" OR _type STREQUAL "MODULE_LIBRARY")
            list(APPEND _runtime_files "$<TARGET_FILE:${_target}>")
            list(APPEND _linked_files "$<TARGET_FILE:${_target}>")
        endif ()
        get_target_property(_companions "${_target}" INTERFACE_DYN_LIBS)
        if (_companions)
            list(APPEND _runtime_files ${_companions})
        endif ()
    endforeach ()
    get_property(_subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach (_subdirectory IN LISTS _subdirectories)
        _sky_collect_imported_runtimes("${_subdirectory}" _child_files _child_linked_files)
        list(APPEND _runtime_files ${_child_files})
        list(APPEND _linked_files ${_child_linked_files})
    endforeach ()
    set(${result} "${_runtime_files}" PARENT_SCOPE)
    if (ARGC GREATER 2)
        set(${ARGV2} "${_linked_files}" PARENT_SCOPE)
    endif ()
endfunction()

# Runtime modules and RHI backends are loaded dynamically. Target dependencies
# alone build them but do not place them inside the app's executable directory.
get_target_property(_sky_launcher_bundle Launcher MACOSX_BUNDLE)
if (APPLE AND _sky_launcher_bundle)
    function(_sky_collect_runtime_libraries directory result)
        get_property(_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
        set(_runtime_targets "")
        foreach (_target IN LISTS _targets)
            get_target_property(_type "${_target}" TYPE)
            if (_type STREQUAL "SHARED_LIBRARY" OR _type STREQUAL "MODULE_LIBRARY")
                list(APPEND _runtime_targets "${_target}")
            endif ()
        endforeach ()
        get_property(_subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
        foreach (_subdirectory IN LISTS _subdirectories)
            _sky_collect_runtime_libraries("${_subdirectory}" _child_targets)
            list(APPEND _runtime_targets ${_child_targets})
        endforeach ()
        set(${result} "${_runtime_targets}" PARENT_SCOPE)
    endfunction()

    _sky_collect_runtime_libraries("${CMAKE_CURRENT_SOURCE_DIR}" _sky_runtime_targets)
    foreach (_target IN LISTS _sky_runtime_targets)
        add_dependencies({{PROJECT_NAME}}Deploy "${_target}")
        get_target_property(_is_framework "${_target}" FRAMEWORK)
        if (CMAKE_SYSTEM_NAME STREQUAL "iOS" AND _is_framework)
            # Let Xcode embed and (for signed exports) sign the complete bundle.
            set_property(TARGET Launcher APPEND PROPERTY XCODE_EMBED_FRAMEWORKS "${_target}")
            set_target_properties(Launcher PROPERTIES
                XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY TRUE
                XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY TRUE
            )
        elseif (CMAKE_SYSTEM_NAME STREQUAL "iOS")
            message(FATAL_ERROR "iOS runtime target ${_target} must be an embedded framework or a static module.")
        else ()
            add_custom_command(TARGET {{PROJECT_NAME}}Deploy POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${_target}>"
                    "$<TARGET_FILE_DIR:Launcher>/$<TARGET_FILE_NAME:${_target}>"
                VERBATIM
            )
        endif ()
    endforeach ()

    if (NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        # Imported runtimes are not build targets and may not relink when the
        # app is rebuilt. Stage them in Deploy on every invocation, including
        # runtime-loaded companions and the real filenames behind symlinks.
        _sky_collect_imported_runtimes("${CMAKE_CURRENT_SOURCE_DIR}" _sky_imported_files)
        set(_sky_copy_runtime_script "${CMAKE_CURRENT_BINARY_DIR}/CopyImportedRuntimes-$<CONFIG>.cmake")
        file(GENERATE OUTPUT "${_sky_copy_runtime_script}" CONTENT
"set(_runtime_files \"${_sky_imported_files}\")
file(MAKE_DIRECTORY \"$<TARGET_FILE_DIR:Launcher>\")
foreach(_runtime IN LISTS _runtime_files)
    if(NOT EXISTS \"\${_runtime}\")
        message(FATAL_ERROR \"Imported runtime is missing: \${_runtime}\")
    endif()
    file(COPY \"\${_runtime}\" DESTINATION \"$<TARGET_FILE_DIR:Launcher>\" FOLLOW_SYMLINK_CHAIN)
endforeach()
")
        add_custom_command(TARGET {{PROJECT_NAME}}Deploy POST_BUILD
            COMMAND ${CMAKE_COMMAND} -P "${_sky_copy_runtime_script}"
            VERBATIM
        )
    endif ()
endif ()

# AGP discovers linked imported libraries itself. This manifest also lists the
# runtime-loaded companions so Gradle can stage just those into generated jniLibs.
if (ANDROID)
    _sky_collect_imported_runtimes("${CMAKE_CURRENT_SOURCE_DIR}" _sky_imported_files _sky_linked_files)
    set(_sky_runtime_manifest "${CMAKE_CURRENT_SOURCE_DIR}/../build/android/runtime-libraries/$<CONFIG>/${CMAKE_ANDROID_ARCH_ABI}/manifest.json")
    file(GENERATE OUTPUT "${_sky_runtime_manifest}" CONTENT
"{
    \"libraries\": [\"$<JOIN:${_sky_imported_files},\",\">\"],
    \"linkedLibraries\": [\"$<JOIN:${_sky_linked_files},\",\">\"]
}
")
endif ()
