file(GLOB_RECURSE INC_FILES include/*)

# Bridge + render module (aurora -> framework), analog of legacy RenderAdaptor:
# reflection + assets + components + the IModule implementation.
sky_add_library(TARGET Aurora.Adaptor STATIC
    SOURCES
        src/AuroraModule.cpp
        src/AuroraReflection.cpp
        ${INC_FILES}
    PUBLIC_INC
        include
    LINK_LIBS
        Aurora
        Aurora.RHI
        Framework
        Core
)

# Launcher dynamic module (AuroraRender); registers the module.
sky_add_library(TARGET AuroraRender SHARED
    SOURCES
        src/AuroraRegistry.cpp
    PUBLIC_INC
        include
    LINK_LIBS
        Aurora.Adaptor
        Core
)

# backends are dlopen'd by Instance at runtime; only need them built + deployed
if (TARGET AuroraVulkan)
    add_dependencies(AuroraRender AuroraVulkan)
endif()
if (TARGET AuroraDX12)
    add_dependencies(AuroraRender AuroraDX12)
endif()
if (TARGET AuroraMetal)
    add_dependencies(AuroraRender AuroraMetal)
endif()

# build AuroraRender before the launcher so its dll lands next to the exe
sky_add_dependency(TARGET AuroraRender DEPENDENCIES Launcher)
