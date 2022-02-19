if (WIN32)
    file(GLOB_RECURSE PLATFORM_SRC windows/*)
elseif(APPLE)
    file(GLOB_RECURSE PLATFORM_SRC macos/*)
endif()

sky_add_exe(TARGET Launcher ${PLATFORM_TYPE}
    SOURCES
        ${PLATFORM_SRC}
    LIBS
        Framework
        ${PLATFORM_EXT_LIBS}
)

add_dependencies(Launcher SkyEngineModule SampleModule)