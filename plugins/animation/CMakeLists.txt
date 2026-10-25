add_subdirectory(core)
add_subdirectory(adaptor)

if (SKY_DEVELOP)
    add_subdirectory(builder)
    if (SKY_BUILD_SANDBOX)
        add_subdirectory(editor)
    endif ()
endif ()
