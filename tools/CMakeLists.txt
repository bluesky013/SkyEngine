# Out-of-process cook worker: built independent of SKY_BUILD_TOOL.
add_subdirectory(asset_tool)

if (SKY_BUILD_TOOL)
#    add_subdirectory(asset_builder)
endif ()
#add_subdirectory(assettools)
#add_subdirectory(noise)
#add_subdirectory(terrain)
#if (SKY_BUILD_PERF_TOOL)
#    add_subdirectory(perf)
#endif()
