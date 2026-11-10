include(${CMAKE_CURRENT_LIST_DIR}/source_ownership.cmake)

function(sky_add_exe)
    cmake_parse_arguments(TMP
        "WIN32;MACOSX_BUNDLE"
        "TARGET"
        "SOURCES;INCS;LIBS"
        ${ARGN}
    )

    if (NOT TMP_TARGET)
        message("target not set")
    endif()

    unset(TMP_TYPE)
    if (TMP_WIN32)
        set(TMP_TYPE WIN32)
    elseif(TMP_MACOSX_BUNDLE)
        set(TMP_TYPE MACOSX_BUNDLE)
    endif ()

    add_executable(${TMP_TARGET} ${TMP_TYPE} ${TMP_SOURCES})
    foreach (dep ${TMP_LINK_LIBS})
        if (TARGET ${dep})
            get_target_property(type ${dep} TYPE)
            if (${type} STREQUAL "INTERFACE_LIBRARY")
                get_target_property(tmpInc ${dep} INTERFACE_INCLUDE_DIRECTORIES)
                get_target_property(tmpLib ${dep} INTERFACE_LINK_LIBRARIES)
                list(APPEND TMP_INCS ${tmpInc})
                list(APPEND TMP_LIBS ${tmpLib})
            endif()
        endif()
    endforeach()
    target_include_directories(${TMP_TARGET} PRIVATE ${TMP_INCS})
    target_link_libraries(${TMP_TARGET} ${TMP_LIBS})
    if(CURRENT_FOLDER)
        set_target_properties(${TMP_TARGET} PROPERTIES FOLDER ${CURRENT_FOLDER})
    endif ()


endfunction()

function(sky_add_library)
    cmake_parse_arguments(TMP
        "STATIC;SHARED"
        "TARGET"
        "SOURCES;PRIVATE_INC;PUBLIC_INC;LINK_LIBS;PUBLIC_LINK_LIBS;PRIVATE_LINK_LIBS;INSTALL_DIR;PUBLIC_DEFS;PRIVATE_DEFS"
        ${ARGN}
    )

    if (NOT TMP_TARGET)
        message("target not set")
    endif()

    unset(TMP_TYPE)
    if (TMP_STATIC)
        set(TMP_TYPE STATIC)
    elseif(TMP_SHARED)
        set(TMP_TYPE SHARED)
    endif()

    add_library(${TMP_TARGET} ${TMP_TYPE}
        ${TMP_SOURCES}
    )

    set(LINK_LIBRARIES ${TMP_LINK_LIBS})
    foreach (dep ${TMP_LINK_LIBS})
        if (TARGET ${dep})
            get_target_property(type ${dep} TYPE)
            if (${type} STREQUAL "INTERFACE_LIBRARY")
                get_target_property(tmpInc ${dep} INTERFACE_INCLUDE_DIRECTORIES)
                if (tmpInc)
                    list(APPEND TMP_PUBLIC_INC ${tmpInc})
                endif()
                get_target_property(tmpLib ${dep} INTERFACE_LINK_LIBRARIES)
                if (tmpLib)
                    list(APPEND LINK_LIBRARIES ${tmpLib})
                endif()

                get_target_property(tmpDlls ${dep} INTERFACE_DYN_LIBS)
                if (tmpDlls)
                    foreach(dll ${tmpDlls})
                        get_filename_component(dllName ${dll} NAME)
                        add_custom_command(TARGET ${TMP_TARGET} POST_BUILD
                                COMMAND ${CMAKE_COMMAND} -E copy
                                ${dll}
                                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${dllName}
                                )
                    endforeach()
                endif()
            endif()
        endif()
    endforeach()

    target_include_directories(${TMP_TARGET} PRIVATE ${TMP_PRIVATE_INC})
    target_include_directories(${TMP_TARGET} PUBLIC ${TMP_PUBLIC_INC})
    if (TMP_PUBLIC_LINK_LIBS OR TMP_PRIVATE_LINK_LIBS)
        target_link_libraries(${TMP_TARGET}
            PUBLIC ${LINK_LIBRARIES} ${TMP_PUBLIC_LINK_LIBS}
            PRIVATE ${TMP_PRIVATE_LINK_LIBS})
    else ()
        target_link_libraries(${TMP_TARGET} ${LINK_LIBRARIES})
    endif ()
    if(CURRENT_FOLDER)
        set_target_properties(${TMP_TARGET} PROPERTIES FOLDER ${CURRENT_FOLDER})
    endif ()
    target_compile_definitions(${TMP_TARGET}
      PUBLIC
        ${TMP_PUBLIC_DEFS}
      PRIVATE
        ${TMP_PRIVATE_DEFS}
    )

    if (TMP_INSTALL_DIR)
        install(TARGETS ${TMP_TARGET}
            EXPORT SkyEngineExport
            ARCHIVE DESTINATION lib
            LIBRARY DESTINATION lib
            RUNTIME DESTINATION bin
            INCLUDES DESTINATION include
        )

        install(DIRECTORY ${TMP_INSTALL_DIR}/
            DESTINATION include/
            FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl"
        )
    endif ()


endfunction()

function(sky_add_test)
    cmake_parse_arguments(TMP
        ""
        "TARGET;WORKING_DIR"
        "SOURCES;INCS;LIBS"
        ${ARGN}
        )

    if (NOT TMP_TARGET)
        message("target not set")
    endif()

    add_executable(${TMP_TARGET} ${TMP_TYPE} ${TMP_SOURCES})
    set_property(TARGET ${TMP_TARGET} PROPERTY SKY_TEST_TARGET TRUE)
    foreach (dep ${TMP_LINK_LIBS})
        if (TARGET ${dep})
            get_target_property(type ${dep} TYPE)
            if (${type} STREQUAL "INTERFACE_LIBRARY")
                get_target_property(tmpInc ${dep} INTERFACE_INCLUDE_DIRECTORIES)
                get_target_property(tmpLib ${dep} INTERFACE_LINK_LIBRARIES)
                list(APPEND TMP_INCS ${tmpInc})
                list(APPEND TMP_LIBS ${tmpLib})
            endif()
        endif()
    endforeach()
    target_include_directories(${TMP_TARGET} PRIVATE ${TMP_INCS})
    target_link_libraries(${TMP_TARGET} ${TMP_LIBS})
    add_test(
        NAME ${TMP_TARGET}
        COMMAND ${TMP_TARGET}
        WORKING_DIRECTORY ${TMP_WORKING_DIR}
    )
    if(CURRENT_FOLDER)
        set_target_properties(${TMP_TARGET} PROPERTIES FOLDER ${CURRENT_FOLDER})
    endif ()
endfunction()

function(sky_add_dependency)
    cmake_parse_arguments(TMP
        ""
        "TARGET"
        "DEPENDENCIES"
        ${ARGN}
        )

    if (NOT TMP_TARGET)
        message("target not set")
    endif()

    foreach (dep ${TMP_DEPENDENCIES})
        get_property(CURR_PROP GLOBAL PROPERTY ${dep}_DEP)
        list(APPEND CURR_PROP ${TMP_TARGET})
        set_property(GLOBAL PROPERTY ${dep}_DEP "${CURR_PROP}")
    endforeach()
endfunction()

function(sky_set_dependency)
    cmake_parse_arguments(TMP
        ""
        "TARGET"
        ""
        ${ARGN}
        )

    if (TMP_TARGET)
        get_property(CURR_PROP GLOBAL PROPERTY ${TMP_TARGET}_DEP)
        if (CURR_PROP)
            add_dependencies(${TMP_TARGET} ${CURR_PROP})
        endif()
    endif()
endfunction()
# Runtime module library. Dynamic packaging (default): SHARED, loaded via dlopen
# (REGISTER_MODULE). Static packaging (`SKY_MODULE_PACKAGING=static`): STATIC with
# SKY_MODULE_STATIC (tagged factory), registered by a generated registration cpp and
# linked into the host. Registers static targets in the global SKY_STATIC_MODULE_TARGETS.
function(sky_add_module)
    cmake_parse_arguments(TMP
        ""
        "TARGET"
        "SOURCES;PRIVATE_INC;PUBLIC_INC;LINK_LIBS;PUBLIC_LINK_LIBS;PRIVATE_LINK_LIBS;INSTALL_DIR;PUBLIC_DEFS;PRIVATE_DEFS"
        ${ARGN}
    )

    if (NOT TMP_TARGET)
        message("target not set")
    endif()

    if (SKY_MODULE_PACKAGING STREQUAL "static")
        set(_sky_mod_type STATIC)
        set(_sky_mod_defs ${TMP_PRIVATE_DEFS} SKY_MODULE_STATIC)
        set_property(GLOBAL APPEND PROPERTY SKY_STATIC_MODULE_TARGETS ${TMP_TARGET})
    else()
        set(_sky_mod_type SHARED)
        set(_sky_mod_defs ${TMP_PRIVATE_DEFS})
    endif()

    sky_add_library(TARGET ${TMP_TARGET} ${_sky_mod_type}
        SOURCES ${TMP_SOURCES}
        PRIVATE_INC ${TMP_PRIVATE_INC}
        PUBLIC_INC ${TMP_PUBLIC_INC}
        LINK_LIBS ${TMP_LINK_LIBS}
        PUBLIC_LINK_LIBS ${TMP_PUBLIC_LINK_LIBS}
        PRIVATE_LINK_LIBS ${TMP_PRIVATE_LINK_LIBS}
        PUBLIC_DEFS ${TMP_PUBLIC_DEFS}
        PRIVATE_DEFS ${_sky_mod_defs}
        INSTALL_DIR ${TMP_INSTALL_DIR}
    )

    if (SKY_MODULE_PACKAGING STREQUAL "static")
        # Tag = target name with non-identifier chars replaced by '_'; used by
        # REGISTER_MODULE_TAGGED to name the factory the generated registration calls.
        string(REPLACE "." "_" _sky_mod_tag "${TMP_TARGET}")
        string(REPLACE "-" "_" _sky_mod_tag "${_sky_mod_tag}")
        target_compile_definitions(${TMP_TARGET} PRIVATE SKY_MODULE_TAG=${_sky_mod_tag})
    endif()
endfunction()

# Generates RegisterStaticModules.cpp from the statically-built runtime modules
# (targets added via sky_add_module under SKY_MODULE_PACKAGING=static; target name =
# module id = factory tag). Provides SkyEngineRegisterStaticModules(ModuleManager&),
# called by the host before LoadModules.
function(sky_generate_static_modules OUT_CPP)
    if (NOT SKY_MODULE_PACKAGING STREQUAL "static")
        return()
    endif()

    get_property(_targets GLOBAL PROPERTY SKY_STATIC_MODULE_TARGETS)
    list(REMOVE_DUPLICATES _targets)

    set(_decls "")
    set(_calls "")
    foreach(_t ${_targets})
        string(REPLACE "." "_" _tag "${_t}")
        string(REPLACE "-" "_" _tag "${_tag}")
        string(APPEND _decls "extern \"C\" sky::IModule *sky_module_create_${_tag}();\n")
        string(APPEND _calls "    if (!manager.AddStaticModule(\"${_t}\", [] { return std::unique_ptr<sky::IModule>(sky_module_create_${_tag}()); }))\n        return false;\n")
    endforeach()

    set(_content
"// Generated by sky_generate_static_modules(); do not edit.\n#include <framework/application/ModuleManager.h>\n#include <framework/interface/IModule.h>\n#include <memory>\n\n${_decls}\nextern \"C\" bool SkyEngineRegisterStaticModules(sky::ModuleManager &manager)\n{\n${_calls}    return true;\n}\n")
    file(WRITE "${OUT_CPP}" "${_content}")
    set_source_files_properties("${OUT_CPP}" PROPERTIES GENERATED TRUE)
endfunction()

# Resolve the same required/optional dependency selection used by ModuleManifest.
function(sky_resolve_module_manifest OUT_TARGETS CONFIG)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CONFIG}")
    file(SIZE "${CONFIG}" _size)
    if(_size GREATER 1048576)
        message(FATAL_ERROR "Module manifest is oversized: ${CONFIG}")
    endif()
    file(READ "${CONFIG}" _json)
    string(JSON _root_type TYPE "${_json}")
    string(JSON _modules_type TYPE "${_json}" modules)
    if(NOT _root_type STREQUAL "OBJECT" OR NOT _modules_type STREQUAL "ARRAY")
        message(FATAL_ERROR "Module manifest requires an object with a modules array: ${CONFIG}")
    endif()
    string(JSON _count LENGTH "${_json}" modules)
    if(_count GREATER 1024)
        message(FATAL_ERROR "Too many modules: ${CONFIG}")
    endif()
    get_property(_options GLOBAL PROPERTY SKY_RUNTIME_BUILD_OPTIONS)
    set(_names)
    set(_selected)
    set(_disabled)
    set(_edge_count 0)
    if(_count GREATER 0)
        math(EXPR _last "${_count} - 1")
        foreach(_index RANGE ${_last})
            string(JSON _name_type TYPE "${_json}" modules ${_index} name)
            string(JSON _name GET "${_json}" modules ${_index} name)
            string(LENGTH "${_name}" _length)
            if(NOT _name_type STREQUAL "STRING" OR NOT _name MATCHES "^[A-Za-z0-9_.-]+$" OR _length GREATER 128 OR _name IN_LIST _names)
                message(FATAL_ERROR "Invalid or duplicate module ${_name}: ${CONFIG}")
            endif()
            list(APPEND _names "${_name}")
            string(JSON _option ERROR_VARIABLE _no_option GET "${_json}" modules ${_index} buildOption)
            set(_enabled TRUE)
            if(NOT _no_option)
                if(NOT _option IN_LIST _options)
                    message(FATAL_ERROR "Unknown buildOption ${_option} for ${_name}: ${CONFIG}")
                endif()
                if(NOT ${_option})
                    set(_enabled FALSE)
                endif()
            endif()
            if(_enabled)
                list(APPEND _selected "${_name}")
            else()
                list(APPEND _disabled "${_name}")
            endif()
            set(_unique)
            foreach(_field dependencies optionalDependencies)
                set(_${_field}_${_name})
                string(JSON _type ERROR_VARIABLE _absent TYPE "${_json}" modules ${_index} ${_field})
                if(_absent)
                    continue()
                endif()
                if(NOT _type STREQUAL "ARRAY")
                    message(FATAL_ERROR "${_field} must be an array for ${_name}: ${CONFIG}")
                endif()
                string(JSON _dep_count LENGTH "${_json}" modules ${_index} ${_field})
                if(_dep_count GREATER 0)
                    math(EXPR _dep_last "${_dep_count} - 1")
                    foreach(_dep_index RANGE ${_dep_last})
                        string(JSON _dep_type TYPE "${_json}" modules ${_index} ${_field} ${_dep_index})
                        string(JSON _dep GET "${_json}" modules ${_index} ${_field} ${_dep_index})
                        string(LENGTH "${_dep}" _length)
                        math(EXPR _edge_count "${_edge_count} + 1")
                        if(NOT _dep_type STREQUAL "STRING" OR NOT _dep MATCHES "^[A-Za-z0-9_.-]+$" OR
                           _length GREATER 128 OR _dep IN_LIST _unique OR _edge_count GREATER 4096)
                            message(FATAL_ERROR "Invalid or excessive dependencies for ${_name}: ${CONFIG}")
                        endif()
                        list(APPEND _unique "${_dep}")
                        list(APPEND _${_field}_${_name} "${_dep}")
                    endforeach()
                endif()
            endforeach()
        endforeach()
    endif()
    foreach(_name IN LISTS _selected)
        foreach(_dep IN LISTS _optionalDependencies_${_name})
            if(NOT _dep IN_LIST _names)
                message(FATAL_ERROR "Unknown optional dependency ${_dep} for ${_name}: ${CONFIG}")
            endif()
            if(NOT _dep IN_LIST _disabled)
                list(APPEND _dependencies_${_name} "${_dep}")
            endif()
        endforeach()
    endforeach()
    set(_closure ${_selected})
    foreach(_name IN LISTS _selected)
        foreach(_dep IN LISTS _dependencies_${_name})
            if(_dep IN_LIST _disabled)
                message(FATAL_ERROR "Module ${_name} requires disabled module ${_dep}: ${CONFIG}")
            endif()
            list(APPEND _closure "${_dep}")
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES _closure)
    foreach(_name IN LISTS _closure)
        if(NOT TARGET ${_name})
            message(FATAL_ERROR "Enabled module target is unavailable: ${_name} (${CONFIG})")
        endif()
    endforeach()
    set(_pending ${_closure})
    while(NOT "${_pending}" STREQUAL "")
        set(_progress FALSE)
        foreach(_name IN LISTS _pending)
            set(_ready TRUE)
            foreach(_dep IN LISTS _dependencies_${_name})
                if(_dep IN_LIST _pending)
                    set(_ready FALSE)
                endif()
            endforeach()
            if(_ready)
                list(REMOVE_ITEM _pending "${_name}")
                set(_progress TRUE)
            endif()
        endforeach()
        if(NOT _progress)
            message(FATAL_ERROR "Module manifest contains a dependency cycle: ${CONFIG}")
        endif()
    endwhile()
    foreach(_name IN LISTS _selected)
        if(NOT "${_dependencies_${_name}}" STREQUAL "")
            add_dependencies(${_name} ${_dependencies_${_name}})
        endif()
    endforeach()
    set(${OUT_TARGETS} "${_closure}" PARENT_SCOPE)
endfunction()
