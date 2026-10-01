file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET Aurora.Core.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        Aurora.RHI.Static
        3rdParty::rapidjson
    INSTALL_DIR
        include
)

file(GLOB_RECURSE TEST_FILES test/*)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Aurora.CoreTest
        SOURCES
            ${TEST_FILES}
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test/main.cpp
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test
        LIBS
            Aurora.Core.Static
            Aurora.RHI.Static
            3rdParty::googletest
    )
endif ()
