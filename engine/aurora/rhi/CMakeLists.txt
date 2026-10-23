file(GLOB_RECURSE TEST_FILES test/*)

add_subdirectory(interface)
add_subdirectory(common)

if (WIN32)
    add_subdirectory(dx12)
endif()

# Vulkan is the runtime backend on desktop/Android; iOS uses Metal only (no
# Vulkan SDK / MoltenVK there).
if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
    add_subdirectory(vulkan)
endif()

if (APPLE)
    add_subdirectory(metal)
endif()

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Aurora.RHITest
        SOURCES
            ${TEST_FILES}
        LIBS
            Aurora.RHI.Static
            Aurora.Pipeline.Static
            3rdParty::googletest
    )

    target_include_directories(Aurora.RHITest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/include)

    # backends are dlopen'd at runtime; only need them built, not linked
    if (TARGET Aurora.Vulkan)
        add_dependencies(Aurora.RHITest Aurora.Vulkan)
    endif()
    if (TARGET Aurora.DX12)
        add_dependencies(Aurora.RHITest Aurora.DX12)
    endif()
    if (TARGET Aurora.Metal)
        add_dependencies(Aurora.RHITest Aurora.Metal)
    endif()

    sky_set_dependency(TARGET Aurora.RHITest)
endif ()