file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET VulkanMemoryAliasing SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        VulkanSampleBase
    )

add_dependencies(Launcher VulkanMemoryAliasing)