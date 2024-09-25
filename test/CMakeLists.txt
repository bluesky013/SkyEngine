
configure_file(EngineRoot.h.in ${CMAKE_BINARY_DIR}/Gen/test/EngineRoot.h)
include_directories(${CMAKE_BINARY_DIR}/Gen)

add_subdirectory(core)
add_subdirectory(framework)
add_subdirectory(vulkan)
add_subdirectory(render)