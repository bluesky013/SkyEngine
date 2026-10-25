set(CURRENT_FOLDER Engine)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET Content.Builder.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Content.Adaptor
        Framework
)

file(GLOB_RECURSE MODULE_SRC module/*)

sky_add_library(TARGET Content.Builder.Module SHARED
    SOURCES
        ${MODULE_SRC}
    LINK_LIBS
        Content.Builder.Static
        Content.Adaptor
        Framework
)
sky_add_dependency(TARGET Content.Builder.Module DEPENDENCIES Launcher SandboxEditor AssetTool)

if (SKY_BUILD_TEST)
    file(GLOB TEST_FILES test/*.cpp)
    sky_add_test(TARGET Content.BuilderTest
        SOURCES ${TEST_FILES}
        LIBS Content.Builder.Static 3rdParty::googletest)
endif ()
