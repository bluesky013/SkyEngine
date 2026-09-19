file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET AuroraRender SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PUBLIC_INC
        include
    LINK_LIBS
        Aurora.RHI
        Framework
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
