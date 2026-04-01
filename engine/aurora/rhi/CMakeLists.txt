file(GLOB_RECURSE TEST_FILES test/*)

add_subdirectory(interface)

if (WIN32)
    add_subdirectory(dx12)
endif()

add_subdirectory(vulkan)

if (APPLE)
    add_subdirectory(metal)
endif()

if (SKY_BUILD_GLES)
    add_subdirectory(gles)
endif()

if (SKY_BUILD_TEST)
    set(AURORA_TEST_LIBS Aurora.RHI 3rdParty::googletest)

    if (TARGET AuroraVulkan)
        list(APPEND AURORA_TEST_LIBS AuroraVulkan)
    endif()
    if (TARGET AuroraDX12)
        list(APPEND AURORA_TEST_LIBS AuroraDX12)
    endif()
    if (TARGET AuroraMetal)
        list(APPEND AURORA_TEST_LIBS AuroraMetal)
    endif()
    if (SKY_BUILD_GLES AND TARGET AuroraGL)
        list(APPEND AURORA_TEST_LIBS AuroraGL)
    endif()

    sky_add_test(TARGET AuroraTest
        SOURCES
        ${TEST_FILES}
        LIBS
        ${AURORA_TEST_LIBS}
    )

    if (TARGET AuroraGL)
        target_compile_definitions(AuroraTest PRIVATE SKY_AURORA_HAS_GLES=1)
    endif()

    sky_set_dependency(TARGET AuroraTest)
endif ()