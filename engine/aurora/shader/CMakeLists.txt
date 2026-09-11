file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_find_3rd(TARGET slang DIR slang)

sky_add_library(TARGET Aurora.Shader STATIC
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
        3rdParty::slang
)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET AuroraShaderTest
        SOURCES
            ${TEST_FILES}
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test
        LIBS
            Aurora.Shader
            Aurora.RHI
            3rdParty::googletest
    )

    # backends are dlopen'd at runtime; only need them built, not linked
    if (TARGET AuroraVulkan)
        add_dependencies(AuroraShaderTest AuroraVulkan)
    endif()
    if (TARGET AuroraDX12)
        add_dependencies(AuroraShaderTest AuroraDX12)
    endif()
    if (TARGET AuroraMetal)
        add_dependencies(AuroraShaderTest AuroraMetal)
    endif()
endif ()
