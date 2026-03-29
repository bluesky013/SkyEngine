set(LIB_NAME "freetype")
if (APPLE)
    sky_find_3rd(TARGET zlib DIR zlib)
    set(FT_EXT_LIBS 3rdParty::zlib)
endif()

sky_3rd_static(${LIB_NAME}
    INCLUDE_SUBDIR freetype2
    DEBUG_SUFFIX d
    LIBS freetype
    EXT_LIBS ${FT_EXT_LIBS}
)
