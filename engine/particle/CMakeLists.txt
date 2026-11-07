include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/module_resources.cmake)

add_subdirectory(core)

sky_register_module_resources(CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/resources.json)

if (SKY_BUILD_PARTICLE)
    add_subdirectory(assets)
    add_subdirectory(adaptor)
    add_subdirectory(builder)
endif ()
