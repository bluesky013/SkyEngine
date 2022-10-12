if (WIN32)
    file(GLOB_RECURSE PLATFORM_SRC windows/*)
elseif(APPLE)
    file(GLOB_RECURSE PLATFORM_SRC macos/*)
endif()
file(GLOB_RECURSE COMMON_SRC common/*)

if (NOT ANDROID)
    sky_add_exe(TARGET Launcher ${PLATFORM_TYPE}
        SOURCES
            ${PLATFORM_SRC}
            ${COMMON_SRC}
        INCS
            common
        LIBS
            Framework
    )
else()
    add_subdirectory(android/app/src/main/cpp)
endif()