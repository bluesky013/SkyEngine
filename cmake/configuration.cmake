if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
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
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    set(PLATFORM_EXT_LIBS  android log)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(PLATFORM_EXT_LIBS winmm imm32 version setupapi)
endif()
