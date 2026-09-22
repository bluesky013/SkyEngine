file(GLOB_RECURSE CORE_SRC_FILES runtime/core/src/*)
file(GLOB_RECURSE CORE_INC_FILES runtime/core/include/*)

file(GLOB_RECURSE RENDER_SRC_FILES runtime/render/src/*)
file(GLOB_RECURSE RENDER_INC_FILES runtime/render/include/*)

file(GLOB_RECURSE TEST_FILES test/*)

include(${ENGINE_ROOT}/cmake/plugin_config.cmake)

# Render-independent core: PVS grid config, sector storage + streaming, and the
# fail-safe visibility query. No RenderCore / editor dependency, so it (and its
# tests) build on any configuration.
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

# Legacy-render glue: the IRenderSceneCulling implementation, the runtime module
# and the debug visualizer. Isolated from the core so the core does not depend on
# the legacy render stack.
sky_add_library(TARGET PVS.Render STATIC
    SOURCES
        ${RENDER_SRC_FILES}
        ${RENDER_INC_FILES}
    PRIVATE_INC
        runtime/render/src
    PUBLIC_INC
        runtime/render/include
    LINK_LIBS
        PVS.Core
        RenderCore
)

sky_add_library(TARGET PVS SHARED
    SOURCES
        runtime/Registry.cpp
    LINK_LIBS
        PVS.Render
)

sky_plugin_apply_target_dependencies(CONFIG "${CMAKE_CURRENT_LIST_DIR}/plugin.json")

if (SKY_BUILD_TEST)
    sky_add_test(TARGET PVSTest
        SOURCES
            ${TEST_FILES}
        LIBS
            PVS.Core
            3rdParty::googletest
    )
endif ()
