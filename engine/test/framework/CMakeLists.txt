file(GLOB_RECURSE TEST_SRC ./*)

sky_add_test(TARGET FrameworkTest
    SOURCES
        ${TEST_SRC}
    WORKING_DIR
        ${CMAKE_SOURCE_DIR}
    LIBS
        Framework
        3rdParty::rapidjson
        3rdParty::googletest
    )

# The export preset schema manifest (single source of truth); the test reads it
# by absolute path so it does not depend on the working directory.
target_compile_definitions(FrameworkTest PRIVATE
    SKY_EXPORT_PRESET_SCHEMA="${CMAKE_SOURCE_DIR}/configs/export_preset.schema.json")
