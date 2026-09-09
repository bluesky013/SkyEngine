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
    set(AURORA_TEST_LIBS Aurora.RHI Aurora.Pipeline 3rdParty::googletest)

    if (TARGET AuroraVulkan)
        list(APPEND AURORA_TEST_LIBS AuroraVulkan)
    endif()
    if (TARGET AuroraDX12)
        list(APPEND AURORA_TEST_LIBS AuroraDX12)
    endif()
    if (TARGET AuroraMetal)
        list(APPEND AURORA_TEST_LIBS AuroraMetal)
    endif()

    sky_add_test(TARGET AuroraTest
        SOURCES
        ${TEST_FILES}
        LIBS
        ${AURORA_TEST_LIBS}
    )

    sky_set_dependency(TARGET AuroraTest)
endif ()