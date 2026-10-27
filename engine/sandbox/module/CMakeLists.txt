set(CURRENT_FOLDER Sandbox/Module)

# SVG rasterizer for editor UI icons (only the sandbox module references it).
sky_find_3rd(TARGET nanosvg DIR nanosvg)

# Windows: comdlg32 for the "add existing project" file dialog.
if (WIN32)
    set(SANDBOX_MODULE_EXTRA_LIBS comdlg32)
endif ()

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)
list(REMOVE_ITEM SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/SandboxRegistry.cpp)

sky_add_library(TARGET SandboxModule.Static STATIC
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
        3rdParty::nanosvg
        ${SANDBOX_MODULE_EXTRA_LIBS}
)
set_target_properties(SandboxModule.Static PROPERTIES POSITION_INDEPENDENT_CODE ON)
sky_add_library(TARGET SandboxModule SHARED
    SOURCES src/SandboxRegistry.cpp
    LINK_LIBS SandboxModule.Static
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
