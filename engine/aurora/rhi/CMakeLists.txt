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
    sky_add_test(TARGET AuroraRHITest
        SOURCES
            ${TEST_FILES}
        LIBS
            Aurora.RHI
            Aurora.Pipeline
            3rdParty::googletest
    )

    # backends are dlopen'd at runtime; only need them built, not linked
    if (TARGET AuroraVulkan)
        add_dependencies(AuroraRHITest AuroraVulkan)
    endif()
    if (TARGET AuroraDX12)
        add_dependencies(AuroraRHITest AuroraDX12)
    endif()
    if (TARGET AuroraMetal)
        add_dependencies(AuroraRHITest AuroraMetal)
    endif()

    sky_set_dependency(TARGET AuroraRHITest)
endif ()