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
        "SOURCES;PRIVATE_INC;PUBLIC_INC;LINK_LIBS;INSTALL_DIR;PUBLIC_DEFS;PRIVATE_DEFS"
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
    target_link_libraries(${TMP_TARGET} ${LINK_LIBRARIES})
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
        "SOURCES;PRIVATE_INC;PUBLIC_INC;LINK_LIBS;INSTALL_DIR;PUBLIC_DEFS;PRIVATE_DEFS"
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
        string(APPEND _calls "    manager.AddStaticModule(\"${_t}\", std::unique_ptr<sky::IModule>(sky_module_create_${_tag}()));\n")
    endforeach()

    set(_content
"// Generated by sky_generate_static_modules(); do not edit.\n#include <framework/application/ModuleManager.h>\n#include <framework/interface/IModule.h>\n#include <memory>\n\n${_decls}\nextern \"C\" void SkyEngineRegisterStaticModules(sky::ModuleManager &manager)\n{\n${_calls}}\n")
    file(WRITE "${OUT_CPP}" "${_content}")
endfunction()
