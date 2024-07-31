if (SKY_EDITOR)
    add_subdirectory(guizmo)
endif ()

if (SKY_BUILD_PYTHON)
    add_subdirectory(python)
endif ()

if (SKY_BUILD_XR)
    add_subdirectory(xr)
endif ()