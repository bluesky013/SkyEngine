# Project-owned Windows launcher.
sky_add_exe(TARGET Launcher WIN32
    SOURCES
        main.cpp
    LIBS
        Framework
)
set_target_properties(Launcher PROPERTIES OUTPUT_NAME "{{PROJECT_NAME}}")
if (MSVC)
    # The GUI launcher shares the main(argc, argv) entry used by other hosts.
    target_link_options(Launcher PRIVATE /ENTRY:mainCRTStartup)
endif ()
