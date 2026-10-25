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
