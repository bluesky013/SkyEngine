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
        EditorRender
)

# backends are dlopen'd by Instance at runtime; build + deploy them next to the host
if (TARGET AuroraVulkan)
    add_dependencies(SandboxModule AuroraVulkan)
endif ()
if (TARGET AuroraDX12)
    add_dependencies(SandboxModule AuroraDX12)
endif ()
if (TARGET AuroraMetal)
    add_dependencies(SandboxModule AuroraMetal)
endif ()

sky_add_dependency(TARGET SandboxModule DEPENDENCIES SandboxEditor)
