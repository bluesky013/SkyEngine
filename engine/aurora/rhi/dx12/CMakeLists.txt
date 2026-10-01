file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET Aurora.DX12 SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        Aurora.RHI.Static
        3rdParty::d3d12ma
        d3d12
        dxgi
)

sky_add_dependency(TARGET Aurora.DX12 DEPENDENCIES Launcher Editor AuroraTest)
