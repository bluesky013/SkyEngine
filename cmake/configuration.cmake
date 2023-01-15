if (APPLE)
    set(PLATFORM_EXT_LIBS
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreAudio"
        "-framework CoreFoundation")
elseif (ANDROID)
    set(PLATFORM_EXT_LIBS
        android
        log)
endif()