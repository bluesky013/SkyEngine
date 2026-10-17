file(GLOB_RECURSE CORE_SRC_FILES runtime/core/src/*)
file(GLOB_RECURSE CORE_INC_FILES runtime/core/include/*)
file(GLOB_RECURSE TEST_FILES test/*)

# PVS is a render-independent visibility core: grid config, sector storage +
# streaming, and the fail-safe visibility query.
if (SKY_MOBILE)
    return()
endif ()

sky_add_library(TARGET PVS.Core STATIC
    SOURCES
        ${CORE_SRC_FILES}
        ${CORE_INC_FILES}
    PRIVATE_INC
        runtime/core/src
    PUBLIC_INC
        runtime/core/include
    LINK_LIBS
        Core
        Framework
)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET PVSTest
        SOURCES
            ${TEST_FILES}
        LIBS
            PVS.Core
            3rdParty::googletest
    )
endif ()
