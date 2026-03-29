set(LIB_NAME "SPIRVCross")
if (MSVC)
    set(DBG_SUFFIX d)
endif()

sky_3rd_static(${LIB_NAME}
    DEBUG_SUFFIX ${DBG_SUFFIX}
    LIBS spirv-cross-glsl spirv-cross-msl spirv-cross-cpp
         spirv-cross-core spirv-cross-reflect spirv-cross-util
)
