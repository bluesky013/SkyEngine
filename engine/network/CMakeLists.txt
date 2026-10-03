set(CURRENT_FOLDER Engine)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)
file(GLOB_RECURSE TEST_FILES test/*)

sky_add_library(TARGET Network STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
)

# Optional OpenSSL-backed HMAC for resume tokens. Falls back to the built-in HMAC-SHA256 when absent.
if (SKY_BUILD_OPENSSL AND TARGET 3rdParty::openssl)
    target_link_libraries(Network PRIVATE 3rdParty::openssl)
    target_compile_definitions(Network PRIVATE SKY_NETWORK_OPENSSL)
endif ()

if (SKY_BUILD_TEST)
    sky_add_test(TARGET NetworkTest
        SOURCES
            ${TEST_FILES}
        INCS
            src
        LIBS
            Network
            3rdParty::googletest
        WORKING_DIR
            $<TARGET_FILE_DIR:NetworkTest>
        )
endif ()

add_subdirectory(replication)
add_subdirectory(ecs)
add_subdirectory(world)
add_subdirectory(lockstep)
