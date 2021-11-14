if (WIN32)
#    set(PLATFORM_TYPE WIN32)
    file(GLOB_RECURSE PLATFORM_SRC windows/*)
endif()

sky_add_exe(TARGET Launcher ${PLATFORM_TYPE}
    SOURCES
        ${PLATFORM_SRC}
    LIBS
        Application
)

add_dependencies(Launcher SkyEngine)