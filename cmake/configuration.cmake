if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(PLATFORM_EXT_LIBS
        iconv
        "-framework AudioToolbox"
        "-framework Carbon"
        "-framework Cocoa"
        "-framework CoreAudio"
        "-framework CoreHaptics"
        "-framework CoreVideo"
        "-framework ForceFeedback"
        "-framework GameController"
        "-framework IOKit"
        "-framework Metal")
    set(PLATFORM_BUNDLE MACOSX_BUNDLE)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
    set(PLATFORM_BUNDLE MACOSX_BUNDLE)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    find_library(log-lib log)

    set(PLATFORM_EXT_LIBS  android ${log-lib})
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(PLATFORM_EXT_LIBS winmm imm32 version setupapi)
    add_compile_definitions(NOMINMAX)
endif()

if (SKY_EDITOR OR SKY_BUILD_TOOL)
    add_compile_definitions(SKY_EDITOR)
endif ()

add_compile_definitions("$<$<CONFIG:Debug>:_DEBUG;DEBUG>")
