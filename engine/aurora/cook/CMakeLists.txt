add_subdirectory(image)

sky_add_library(TARGET Aurora.Cook SHARED
    SOURCES
        module/AuroraCookModule.cpp
    LINK_LIBS
        AuroraCook.Static
        AudioBuilder.Static
    )
sky_add_dependency(TARGET Aurora.Cook DEPENDENCIES Launcher Editor)

if (SKY_BUILD_TEST)
    file(GLOB_RECURSE TEST_FILES test/*)
    sky_add_test(TARGET AuroraCookTest
        SOURCES
            ${TEST_FILES}
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test/main.cpp
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test
        LIBS
            AuroraCook.Static
            Aurora.RHI
            Core
            3rdParty::googletest
        )
endif ()
