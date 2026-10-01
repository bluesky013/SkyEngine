# Runtime render module (launcher-facing dynamic module), symmetric with
# engine/aurora/editor. Links the static bridge and registers AuroraModule.
sky_add_library(TARGET Aurora.Render SHARED
    SOURCES
        src/AuroraRegistry.cpp
    LINK_LIBS
        Aurora.Adaptor.Static
        Core
)

# backends are dlopen'd by Instance at runtime; only need them built + deployed
if (TARGET Aurora.Vulkan)
    add_dependencies(Aurora.Render Aurora.Vulkan)
endif()
if (TARGET Aurora.DX12)
    add_dependencies(Aurora.Render Aurora.DX12)
endif()
if (TARGET Aurora.Metal)
    add_dependencies(Aurora.Render Aurora.Metal)
endif()

# build Aurora.Render before the launcher so its dll lands next to the exe
sky_add_dependency(TARGET Aurora.Render DEPENDENCIES Launcher)
