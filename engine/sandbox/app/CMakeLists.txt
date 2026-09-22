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

# Deploy the editor's builtin resources (icons, fonts, ...) next to the
# executable as `resources/`, so the sandbox resolves them without the
# repo-root project assets. Prefers a symlink (no duplication for large
# resource trees) with an automatic copy fallback.
add_custom_command(TARGET SandboxEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        "-DSRC_DIR=${CMAKE_SOURCE_DIR}/engine/sandbox/resources"
        "-DDST_DIR=$<TARGET_FILE_DIR:SandboxEditor>/resources"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/deploy_resources.cmake
    COMMENT "Deploying editor builtin resources"
)
