if (COMMAND sky_plugin_apply_target_dependencies)
    return()
endif()

function(sky_plugin_apply_target_dependencies)
    cmake_parse_arguments(TMP
        ""
        "CONFIG"
        ""
        ${ARGN}
    )

    if (NOT TMP_CONFIG)
        message(FATAL_ERROR "sky_plugin_apply_target_dependencies: CONFIG is required")
    endif()

    if (NOT EXISTS "${TMP_CONFIG}")
        message(FATAL_ERROR "sky_plugin_apply_target_dependencies: config not found: ${TMP_CONFIG}")
    endif()

    file(READ "${TMP_CONFIG}" _plugin_json)
    string(JSON _target_count LENGTH "${_plugin_json}" "targets")

    if (_target_count EQUAL 0)
        return()
    endif()

    math(EXPR _target_last "${_target_count} - 1")
    foreach(_ti RANGE ${_target_last})
        string(JSON _target_name GET "${_plugin_json}" "targets" "${_ti}" "name")
        string(JSON _dep_count ERROR_VARIABLE _dep_count_error
               LENGTH "${_plugin_json}" "targets" "${_ti}" "dependencies")

        set(_deps)
        if ((NOT _dep_count_error) AND (_dep_count GREATER 0))
            math(EXPR _dep_last "${_dep_count} - 1")
            foreach(_di RANGE ${_dep_last})
                string(JSON _dep GET "${_plugin_json}" "targets" "${_ti}" "dependencies" "${_di}")
                list(APPEND _deps "${_dep}")
            endforeach()
        endif()

        if (_deps)
            sky_add_dependency(TARGET ${_target_name} DEPENDENCIES ${_deps})
        endif()
    endforeach()
endfunction()
