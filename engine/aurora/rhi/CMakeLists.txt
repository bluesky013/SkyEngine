file(GLOB_RECURSE TEST_FILES test/*)

add_subdirectory(interface)
add_subdirectory(dx12)
add_subdirectory(vulkan)

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
    if (SKY_BUILD_GLES AND TARGET AuroraGL)
        list(APPEND AURORA_TEST_LIBS AuroraGL)
    endif()

    sky_add_test(TARGET AuroraTest
        SOURCES
        ${TEST_FILES}
        LIBS
        ${AURORA_TEST_LIBS}
    )
    sky_set_dependency(TARGET AuroraTest)
endif ()