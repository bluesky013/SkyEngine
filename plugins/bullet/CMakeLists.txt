file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

include(${ENGINE_ROOT}/cmake/plugin_config.cmake)

sky_find_3rd(TARGET bullet3 DIR bullet3)

sky_add_library(TARGET BulletPhysicsModule SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        BulletModule.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Physics
        3rdParty::bullet3
)

# Aurora render bridge module: registers the physics mesh provider with an aurora-mesh converter.
# Lives in the bullet plugin so the physics core stays interface-only and legacy render is not used.
sky_add_library(TARGET BulletPhysicsAuroraModule SHARED
    SOURCES
        aurora/BulletPhysicsAuroraModule.cpp
    LINK_LIBS
        Physics
        Aurora.Adaptor
        Framework
)

sky_plugin_apply_target_dependencies(CONFIG "${CMAKE_CURRENT_LIST_DIR}/plugin.json")

if (SKY_BUILD_TEST)
    file(GLOB_RECURSE TEST_FILES test/*)
    sky_add_test(TARGET BulletPhysicsTest
        SOURCES
            ${TEST_FILES}
            ${SRC_FILES}
            BulletModule.cpp
        INCS
            include
            src
        LIBS
            Physics
            3rdParty::bullet3
            Framework
            3rdParty::googletest
        WORKING_DIR
            $<TARGET_FILE_DIR:BulletPhysicsTest>
        )
endif ()