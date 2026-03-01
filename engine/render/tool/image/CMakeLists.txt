file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_exe(TARGET SkyImageViewer ${PLATFORM_TYPE}
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    INCS
        include
    LIBS
        RenderBuilder.Static
)

add_dependencies(SkyImageViewer VulkanRHI)