set(LIB_NAME "cpython")
set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")

if (NOT DEFINED CPYTHON_VERSION)
    set(CPYTHON_VERSION "3.13" CACHE STRING "CPython major.minor version")
endif()
string(REPLACE "." "" CPYTHON_TAG "${CPYTHON_VERSION}")

set(${LIB_NAME}_INCLUDE_DIR ${${LIB_NAME}_PATH}/include)

set(_cpython_missing "")

if (MSVC)
    # Static core plus the builtin Tier 1 module archives, per configuration.
    set(${LIB_NAME}_STATIC_RELEASE ${${LIB_NAME}_PATH}/libs/Release/python${CPYTHON_TAG}_static.lib)
    set(${LIB_NAME}_STATIC_DEBUG ${${LIB_NAME}_PATH}/libs/Debug/python${CPYTHON_TAG}_static_d.lib)
    file(GLOB ${LIB_NAME}_MODULE_LIBS_RELEASE ${${LIB_NAME}_PATH}/libs/Release/modules/*.lib)
    file(GLOB ${LIB_NAME}_MODULE_LIBS_DEBUG ${${LIB_NAME}_PATH}/libs/Debug/modules/*.lib)

    foreach (_artifact ${${LIB_NAME}_STATIC_RELEASE} ${${LIB_NAME}_STATIC_DEBUG})
        if (NOT EXISTS ${_artifact})
            list(APPEND _cpython_missing ${_artifact})
        endif()
    endforeach()
    if (NOT ${LIB_NAME}_MODULE_LIBS_RELEASE)
        list(APPEND _cpython_missing "${${LIB_NAME}_PATH}/libs/Release/modules/*.lib")
    endif()
    if (NOT ${LIB_NAME}_MODULE_LIBS_DEBUG)
        list(APPEND _cpython_missing "${${LIB_NAME}_PATH}/libs/Debug/modules/*.lib")
    endif()

    set(${LIB_NAME}_LIBRARY
            "$<$<CONFIG:release>:${${LIB_NAME}_STATIC_RELEASE}>"
            "$<$<CONFIG:debug>:${${LIB_NAME}_STATIC_DEBUG}>")
    set(${LIB_NAME}_MODULE_LIBS
            "$<$<CONFIG:release>:${${LIB_NAME}_MODULE_LIBS_RELEASE}>"
            "$<$<CONFIG:debug>:${${LIB_NAME}_MODULE_LIBS_DEBUG}>")
    set(${LIB_NAME}_SYSTEM_LIBS ws2_32 crypt32 rpcrt4 advapi32 user32 shell32 ole32 oleaut32 version pathcch bcrypt iphlpapi)
else ()
    set(${LIB_NAME}_STATIC_LIBRARY ${${LIB_NAME}_PATH}/lib/libpython${CPYTHON_VERSION}.a)
    if (NOT EXISTS ${${LIB_NAME}_STATIC_LIBRARY})
        list(APPEND _cpython_missing ${${LIB_NAME}_STATIC_LIBRARY})
    endif()
    set(${LIB_NAME}_LIBRARY ${${LIB_NAME}_STATIC_LIBRARY})
    set(${LIB_NAME}_SYSTEM_LIBS pthread dl m)
    if (ANDROID)
        list(APPEND ${LIB_NAME}_SYSTEM_LIBS log)
    endif ()
endif ()

if (NOT EXISTS ${${LIB_NAME}_INCLUDE_DIR})
    list(APPEND _cpython_missing ${${LIB_NAME}_INCLUDE_DIR})
endif ()

if (_cpython_missing)
    message(FATAL_ERROR "cpython package is incomplete at '${${LIB_NAME}_PATH}'.\n"
        "Missing: ${_cpython_missing}\n"
        "Build it with: python python/third_party.py -p <platform> -t cpython")
endif ()

add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_INCLUDE_DIR})
target_compile_definitions(${TARGET_WITH_NAMESPACE} INTERFACE Py_NO_ENABLE_SHARED)
target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE
        ${${LIB_NAME}_LIBRARY}
        ${${LIB_NAME}_MODULE_LIBS}
        ${${LIB_NAME}_SYSTEM_LIBS})
# The CPython objects are built with /GL, so the final link needs /LTCG.
target_link_options(${TARGET_WITH_NAMESPACE} INTERFACE "$<$<CONFIG:Release>:/LTCG>")

set_target_properties(${TARGET_WITH_NAMESPACE} PROPERTIES INTERFACE_DYN_LIBS "")

# The statically built _ssl/_hashlib builtins need the static OpenSSL libraries.
if (SKY_PYTHON_SSL AND TARGET 3rdParty::openssl)
    target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE 3rdParty::openssl)
endif ()

set(${LIB_NAME}_FOUND True)
