file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    file(GLOB_RECURSE PLATFORM_SRC platform/windows/*)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    file(GLOB_RECURSE PLATFORM_SRC platform/posix/*)
else()
    file(GLOB_RECURSE PLATFORM_SRC platform/posix/*)
endif()

include(${ENGINE_ROOT}/cmake/plugin_config.cmake)

sky_add_library(TARGET ConsoleModule SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        ${PLATFORM_SRC}
        Module.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Framework
)

sky_plugin_apply_target_dependencies(CONFIG "${CMAKE_CURRENT_LIST_DIR}/plugin.json")
