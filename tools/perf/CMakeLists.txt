file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_exe(TARGET PerfCat ${PLATFORM_TYPE}
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    INCS
        include
    LIBS
        Framework
        VulkanRHI.Static
        3rdParty::imgui
        3rdParty::implot
    )