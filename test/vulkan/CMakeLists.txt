file(GLOB_RECURSE TEST_SRC ./*)

sky_add_test(TARGET VulkanTest
    SOURCES
        ${TEST_SRC}
    LIBS
        VulkanRHI
        3rdParty::googletest
    )
