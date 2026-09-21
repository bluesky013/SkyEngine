set(CURRENT_FOLDER Sandbox)

# Non-Qt editor rebuild area. `core` is the toolkit- and render-independent
# editor core; `src` is the shell prototype. Nothing here depends on the legacy
# Qt editor (engine/editor).
add_subdirectory(core)

if (SKY_BUILD_SANDBOX)
    # Editor-owned renderer (Aurora + UI); independent of EditorCore.
    add_subdirectory(render)

    file(GLOB_RECURSE SANDBOX_SRC src/*)

    # Prototype shell: drives EditorCore and borrows the native window framework
    # (Framework Platform + NativeWindow) without Qt or render coupling.
    sky_add_exe(TARGET Sandbox
        SOURCES
            ${SANDBOX_SRC}
        INCS
            src
        LIBS
            EditorCore
            Framework
    )
endif ()

if (SKY_BUILD_TEST)
    add_subdirectory(test)
endif ()
