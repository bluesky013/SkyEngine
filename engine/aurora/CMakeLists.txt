set(CURRENT_FOLDER Engine/Aurora)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/module_resources.cmake)
sky_register_module_resources(CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/resources.json")

add_subdirectory(rhi)
add_subdirectory(core)
add_subdirectory(shader)
add_subdirectory(pipeline)
add_subdirectory(adaptor)
add_subdirectory(runtime)

if ((SKY_DEVELOP) AND ((${CMAKE_SYSTEM_NAME} STREQUAL "Darwin") OR (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")))
    add_subdirectory(cook)
endif ()

if (SKY_BUILD_TEST)
    add_subdirectory(core/test/renderer)
endif ()
