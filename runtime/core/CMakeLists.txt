file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

file(GLOB_RECURSE UNIX_ASYNC_SRC platform/unix/async/*)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    file(GLOB_RECURSE CORE_PLATFORM_SRC
        platform/windows/*)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    file(GLOB_RECURSE CORE_PLATFORM_SRC
        platform/macos/*)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    file(GLOB_RECURSE CORE_PLATFORM_SRC
        platform/android/*)
    list(APPEND CORE_PLATFORM_SRC ${UNIX_ASYNC_SRC})
endif ()

set(CORE_OPT_LIBS)
if (SKY_USE_TRACY)
    list(APPEND CORE_OPT_LIBS   3rdParty::tracy)
endif ()

sky_add_library(TARGET Core STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        ${CORE_PLATFORM_SRC}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        3rdParty::sfmt
        3rdParty::crc32
        3rdParty::taskflow
        3rdParty::boost
        3rdParty::rapidjson
#        3rdParty::mimalloc
        ${CORE_OPT_LIBS}
    )
