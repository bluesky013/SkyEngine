sky_add_library(TARGET Launcher SHARED
    SOURCES ohos_main.cpp
    LINK_LIBS Framework ace_ndk.z ace_napi.z
)
set_target_properties(Launcher PROPERTIES OUTPUT_NAME "{{LAUNCHER_LIB_NAME}}")
target_link_options(Launcher PRIVATE -Wl,--no-undefined)
