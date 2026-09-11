file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET Aurora.Pipeline STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        Aurora.RHI
        Aurora
        Aurora.Shader
)

file(GLOB_RECURSE TEST_FILES test/*)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET AuroraPipelineTest
        SOURCES
            ${TEST_FILES}
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test/main.cpp
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test
        LIBS
            Aurora.Pipeline
            3rdParty::googletest
    )
endif ()
