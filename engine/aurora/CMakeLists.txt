set(CURRENT_FOLDER Engine/Aurora)

add_subdirectory(rhi)
add_subdirectory(shader)
add_subdirectory(core)
add_subdirectory(pipeline)
add_subdirectory(adaptor)
add_subdirectory(runtime)
add_subdirectory(editor)

if ((SKY_BUILD_TOOL) AND ((${CMAKE_SYSTEM_NAME} STREQUAL "Darwin") OR (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")))
    add_subdirectory(cook)
endif ()