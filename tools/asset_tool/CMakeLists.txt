sky_add_exe(TARGET AssetTool
    SOURCES
        main.cpp
        CookWorkerHost.cpp
        ProductAudit.cpp
    LIBS
        Framework
)

sky_set_dependency(TARGET AssetTool)

if (TARGET Animation.Builder)
    target_compile_definitions(AssetTool PRIVATE SKY_HAS_ANIMATION_BUILDER=1)
endif ()

if (TARGET JoltPhysics.Builder)
    target_compile_definitions(AssetTool PRIVATE SKY_HAS_JOLT_BUILDER=1)
    add_dependencies(AssetTool JoltPhysicsModule)
endif ()

if (TARGET SkyNavigation.Builder)
    target_compile_definitions(AssetTool PRIVATE SKY_HAS_NAVIGATION_BUILDER=1)
    add_dependencies(AssetTool RecastNavigation)
endif ()
