# Project-owned Linux launcher.
sky_add_exe(TARGET Launcher
    SOURCES
        main.cpp
    LIBS
        Framework
)
set_target_properties(Launcher PROPERTIES
    OUTPUT_NAME "{{PROJECT_NAME}}"
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN"
)
