set(LIB_NAME "openssl")
set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")

set(${LIB_NAME}_INCLUDE_DIR ${${LIB_NAME}_PATH}/include)
set(${LIB_NAME}_LIB_DIR ${${LIB_NAME}_PATH}/lib)

if (MSVC)
    set(_openssl_suffix ".lib")
else ()
    set(_openssl_suffix ".a")
endif ()

# Order matters for static linking: dependents before dependencies.
set(${LIB_NAME}_LIBRARIES
    ${${LIB_NAME}_LIB_DIR}/libssl${_openssl_suffix}
    ${${LIB_NAME}_LIB_DIR}/libcrypto${_openssl_suffix}
    ${${LIB_NAME}_LIB_DIR}/libdefault${_openssl_suffix}
    ${${LIB_NAME}_LIB_DIR}/libcommon${_openssl_suffix}
    ${${LIB_NAME}_LIB_DIR}/liblegacy${_openssl_suffix})

set(_openssl_missing "")
foreach (_artifact ${${LIB_NAME}_INCLUDE_DIR}/openssl/ssl.h ${${LIB_NAME}_LIBRARIES})
    if (NOT EXISTS ${_artifact})
        list(APPEND _openssl_missing ${_artifact})
    endif ()
endforeach ()

if (_openssl_missing)
    message(FATAL_ERROR "openssl package is incomplete at '${${LIB_NAME}_PATH}'.\n"
        "Missing: ${_openssl_missing}\n"
        "Build it with: python python/third_party.py -p <platform> -t openssl")
endif ()

if (WIN32)
    set(${LIB_NAME}_SYSTEM_LIBS ws2_32 crypt32 advapi32 user32 gdi32 bcrypt)
else ()
    set(${LIB_NAME}_SYSTEM_LIBS pthread dl m)
    if (ANDROID)
        list(APPEND ${LIB_NAME}_SYSTEM_LIBS log)
    endif ()
endif ()

add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_INCLUDE_DIR})
target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE
        ${${LIB_NAME}_LIBRARIES}
        ${${LIB_NAME}_SYSTEM_LIBS})

set(${LIB_NAME}_FOUND True)
