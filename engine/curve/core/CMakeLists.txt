file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET Curve STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    PUBLIC_LINK_LIBS
        Core
    INSTALL_DIR
        include
)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET CurveTest
        SOURCES
            ${TEST_FILES}
            ${CMAKE_CURRENT_SOURCE_DIR}/../../aurora/rhi/test/main.cpp
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../../aurora/rhi/test
        LIBS
            Curve
            3rdParty::googletest
    )
endif ()
