file(GLOB_RECURSE TEST_FILES test/*)

add_subdirectory(interface)

if (WIN32)
    add_subdirectory(dx12)
endif()

add_subdirectory(vulkan)

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