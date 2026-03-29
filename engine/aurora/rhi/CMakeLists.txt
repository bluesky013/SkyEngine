file(GLOB_RECURSE TEST_FILES test/*)

add_subdirectory(interface)
add_subdirectory(dx12)
add_subdirectory(vulkan)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET AuroraTest
        SOURCES
        ${TEST_FILES}
        LIBS
        Aurora.RHI
        3rdParty::googletest
    )
    sky_set_dependency(TARGET AuroraTest)
endif ()