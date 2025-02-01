add_subdirectory(render)

sky_add_library(TARGET RenderBuilder SHARED
    SOURCES
        module/BuilderModule.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        RenderBuilder.Static
    )
sky_add_dependency(TARGET RenderBuilder DEPENDENCIES Launcher Editor AssetBuilder)


if (SKY_BUILD_TEST)
    file(GLOB_RECURSE TEST_FILES test/*)
    sky_add_test(TARGET RenderBuilderTest
        SOURCES
            ${TEST_FILES}
        LIBS
            RenderBuilder.Static
            3rdParty::googletest
    )
endif ()