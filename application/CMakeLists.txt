if (WIN32)
    file(GLOB_RECURSE PLATFORM_SRC platform/windows/*)
endif()
file(GLOB_RECURSE APP_SRC src/*)
file(GLOB_RECURSE APP_INC include/*)

message(${PLATFORM_SRC})

sky_add_library(TARGET Application STATIC
    SOURCES
        ${PLATFORM_SRC}
        ${APP_SRC}
        ${APP_INC}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
    )