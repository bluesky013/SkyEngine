file(GLOB_RECURSE BUILDER_SRC src/*)
file(GLOB_RECURSE BUILDER_INC include/*)

#set(ShaderCLib "C:\\VulkanSDK\\1.2.154.1\\Lib\\shaderc_combined.lib")
#set(ShaderCInc "C:\\VulkanSDK\\1.2.154.1\\Include\\shaderc")

sky_add_library(TARGET ShaderBuilder SHARED
    SOURCES
        ShaderModule.cpp
        ${BUILDER_SRC}
        ${BUILDER_INC}
    PRIVATE_INC
        src
        ${ShaderCInc}
    PUBLIC_INC
        include
    LINK_LIBS
        Rapidjson
        SkyEngine
        shaderc
    )