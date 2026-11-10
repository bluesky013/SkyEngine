sky_add_exe(TARGET AssetTool
    SOURCES
        main.cpp
        CookWorkerHost.cpp
        ProductAudit.cpp
    LIBS
        Framework
)

set(_asset_tool_manifests "${CMAKE_CURRENT_SOURCE_DIR}/../../configs/modules_asset_audit.json")
if(SKY_DEVELOP)
    list(APPEND _asset_tool_manifests "${CMAKE_CURRENT_SOURCE_DIR}/../../configs/modules_asset_tool.json")
endif()
set(_asset_tool_modules)
foreach(_manifest IN LISTS _asset_tool_manifests)
    sky_resolve_module_manifest(_modules "${_manifest}")
    list(APPEND _asset_tool_modules ${_modules})
endforeach()
list(REMOVE_DUPLICATES _asset_tool_modules)
if(_asset_tool_modules)
    add_dependencies(AssetTool ${_asset_tool_modules})
endif()
foreach(_module IN LISTS _asset_tool_modules)
    get_target_property(_type ${_module} TYPE)
    if(_type STREQUAL "SHARED_LIBRARY")
        add_custom_command(TARGET AssetTool POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${_module}>" "$<TARGET_FILE_DIR:AssetTool>"
            VERBATIM)
    endif()
endforeach()
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/module_resources.cmake")
sky_runtime_resource_directory(_asset_tool_resource_directory)
foreach(_manifest modules_asset_tool.json modules_asset_audit.json)
    file(GENERATE OUTPUT "${_asset_tool_resource_directory}/configs/${_manifest}"
        INPUT "${CMAKE_CURRENT_SOURCE_DIR}/../../configs/${_manifest}")
endforeach()
if(SKY_MODULE_PACKAGING STREQUAL "static")
    get_property(_all_static_modules GLOBAL PROPERTY SKY_STATIC_MODULE_TARGETS)
    set(_asset_tool_static_modules)
    foreach(_module IN LISTS _asset_tool_modules)
        if(_module IN_LIST _all_static_modules)
            list(APPEND _asset_tool_static_modules "${_module}")
        endif()
    endforeach()
    set_property(GLOBAL PROPERTY SKY_STATIC_MODULE_TARGETS "${_asset_tool_static_modules}")
    sky_generate_static_modules("${CMAKE_CURRENT_BINARY_DIR}/RegisterStaticModules.cpp")
    set_property(GLOBAL PROPERTY SKY_STATIC_MODULE_TARGETS "${_all_static_modules}")
    target_sources(AssetTool PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/RegisterStaticModules.cpp")
    target_link_libraries(AssetTool ${_asset_tool_static_modules})
endif()
