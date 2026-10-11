set(CURRENT_FOLDER Test)

configure_file(EngineRoot.h.in ${CMAKE_BINARY_DIR}/Gen/test/EngineRoot.h)
include_directories(${CMAKE_BINARY_DIR}/Gen)

add_subdirectory(core)
add_subdirectory(framework)
add_subdirectory(process_helper)
add_subdirectory(cook_worker_stub)
# Legacy render/vulkan tests require the deprecated (opt-in) legacy stack.
if (SKY_BUILD_LEGACY_RENDER)
    add_subdirectory(vulkan)
    add_subdirectory(render)
endif ()
add_subdirectory(ui)