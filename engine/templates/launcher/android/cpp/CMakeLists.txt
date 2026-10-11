# Project-owned Android launcher native entry (android_main).
sky_add_library(TARGET Launcher SHARED
    SOURCES
        android_main.cpp
    LINK_LIBS
        Framework
)
set_target_properties(Launcher PROPERTIES OUTPUT_NAME "{{LAUNCHER_LIB_NAME}}")
