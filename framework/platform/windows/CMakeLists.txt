file(GLOB_RECURSE APP_SRC_FILES application/*)
file(GLOB_RECURSE WND_SRC_FILES window/*)

sky_add_library(TARGET FrameworkImpl SHARED
    SOURCES
        ${APP_SRC_FILES}
        ${WND_SRC_FILES}
    LINK_LIBS
        Framework
)