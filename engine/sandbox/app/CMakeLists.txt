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

# GUI subsystem by default: no console window opens on launch. Pass `--console`
# to attach one (see main.cpp). Keep `main` as the entry point.
if (WIN32)
    set_target_properties(SandboxEditor PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_options(SandboxEditor PRIVATE /ENTRY:mainCRTStartup)
endif ()

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

# Deploy the engine bundle (assets/ for the read-only engine mount + configs/ for
# the module list) next to the editor, so the asset DB's engine mount resolves.
add_custom_command(TARGET SandboxEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        "-DSRC_DIR=${CMAKE_SOURCE_DIR}/assets"
        "-DDST_DIR=$<TARGET_FILE_DIR:SandboxEditor>/assets"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/deploy_resources.cmake
    COMMAND ${CMAKE_COMMAND}
        "-DSRC_DIR=${CMAKE_SOURCE_DIR}/configs"
        "-DDST_DIR=$<TARGET_FILE_DIR:SandboxEditor>/configs"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/deploy_resources.cmake
    COMMENT "Deploying engine assets + configs"
)
