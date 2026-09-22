set(CURRENT_FOLDER Sandbox/App)

file(GLOB_RECURSE SRC_FILES src/*)

# Non-Qt editor host: an Application that loads SandboxModule and owns the
# window. Separate from the RHI-free `Sandbox` shell.
sky_add_exe(TARGET SandboxEditor
    SOURCES
        ${SRC_FILES}
    INCS
        src
    LIBS
        Framework
        Core
)
