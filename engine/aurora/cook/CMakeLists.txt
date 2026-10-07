# one AuroraCook.Static for all cookers; each cooker contributes its sources
# under <name>/src + <name>/include and its own third-party deps
file(GLOB_RECURSE SRC_FILES image/src/* mesh/src/*)
file(GLOB_RECURSE INC_FILES image/include/* mesh/include/*)

sky_find_3rd(TARGET stb          DIR stb)
sky_find_3rd(TARGET ispc_texcomp DIR ispc_texcomp)
sky_find_3rd(TARGET astc          DIR astc)
sky_find_3rd(TARGET assimp        DIR assimp)
sky_find_3rd(TARGET meshoptimizer DIR meshoptimizer)

sky_add_library(TARGET AuroraCook.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        image/src
        mesh/src
    PUBLIC_INC
        image/include
        mesh/include
    LINK_LIBS
        Aurora.Adaptor.Static
        Framework
        Core
        3rdParty::stb
        3rdParty::ispc_texcomp
        3rdParty::astc
        3rdParty::assimp
        3rdParty::meshoptimizer
    )

sky_add_library(TARGET Aurora.Cook SHARED
    SOURCES
        module/AuroraCookModule.cpp
    LINK_LIBS
        AuroraCook.Static
    )
sky_add_dependency(TARGET Aurora.Cook DEPENDENCIES Launcher Editor)

if (SKY_BUILD_TEST)
    file(GLOB_RECURSE TEST_FILES test/*)
    sky_add_test(TARGET Aurora.CookTest
        SOURCES
            ${TEST_FILES}
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test/main.cpp
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test
        LIBS
            AuroraCook.Static
            Aurora.RHI.Static
            Core
            3rdParty::googletest
        )
endif ()
