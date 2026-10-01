set(CURRENT_FOLDER Sandbox/Module)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

# Editor module (dynamic, loaded by ModuleManager). A thin adapter over the
# engine application/module system; the rendering lives in EditorRender.
sky_add_library(TARGET SandboxModule SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        Framework
        EditorCore
        EditorShell
        EditorRender
)

# backends are dlopen'd by Instance at runtime; build + deploy them next to the host
if (TARGET Aurora.Vulkan)
    add_dependencies(SandboxModule Aurora.Vulkan)
endif ()
if (TARGET Aurora.DX12)
    add_dependencies(SandboxModule Aurora.DX12)
endif ()
if (TARGET Aurora.Metal)
    add_dependencies(SandboxModule Aurora.Metal)
endif ()

sky_add_dependency(TARGET SandboxModule DEPENDENCIES SandboxEditor)
