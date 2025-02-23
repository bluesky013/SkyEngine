if (SKY_EDITOR)
    add_subdirectory(guizmo)
endif ()

if (SKY_BUILD_PYTHON)
    add_subdirectory(python)
endif ()

if (SKY_BUILD_XR)
    add_subdirectory(xr)
endif ()

if (SKY_BUILD_BULLET)
    add_subdirectory(bullet)
endif ()

if (SKY_BUILD_RECAST)
    add_subdirectory(recast)
endif ()

if (SKY_BUILD_FREETYPE)
    add_subdirectory(freetype)
endif ()

if (SKY_BUILD_TERRAIN)
    add_subdirectory(terrain)
endif ()

if (SKY_BUILD_COMPRESSION)
    add_subdirectory(compression)
endif()