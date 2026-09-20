cmake_minimum_required(VERSION 3.20)
project(openssl_static C)

# Builds the static OpenSSL libraries from the pristine source tree plus the generated files and
# source lists committed under this directory (produced once by python/gen_openssl.py). No Perl and
# no make are required, so this works on every host including Windows -> Android.
#
# Libraries: libcrypto, libssl and the providers (libdefault, libcommon, liblegacy) that OpenSSL's
# static builds keep as separate archives.

set(OPENSSL_SRC "" CACHE PATH "OpenSSL source tree")
set(OPENSSL_VARIANT "unix" CACHE STRING "Generated variant (unix, win64, darwin)")
set(OPENSSL_GENERATED "" CACHE PATH "Directory holding the extracted generated/ tree")
set(OPENSSL_SOURCES "" CACHE PATH "sources-<variant>.cmake inside the extracted tree")
set(OPENSSL_INSTALL_DIR "" CACHE PATH "Install prefix")

set(_openssl_root "${CMAKE_CURRENT_LIST_DIR}")
if (NOT OPENSSL_GENERATED)
    # Development fallback: a generated/ tree next to this file.
    set(OPENSSL_GENERATED "${_openssl_root}/generated")
endif ()
set(_common "${OPENSSL_GENERATED}/common")
set(_variant "${OPENSSL_GENERATED}/${OPENSSL_VARIANT}")

if (NOT OPENSSL_SOURCES)
    set(OPENSSL_SOURCES "${OPENSSL_GENERATED}/sources-${OPENSSL_VARIANT}.cmake")
endif ()
if (NOT EXISTS "${OPENSSL_SOURCES}")
    message(FATAL_ERROR "OpenSSL source list not found: '${OPENSSL_SOURCES}'\n"
        "Build the openssl package (python python/third_party.py -p <platform> -t openssl) to extract it.")
endif ()
if (NOT EXISTS "${_variant}/include/openssl/configuration.h")
    message(FATAL_ERROR "OpenSSL generated variant '${OPENSSL_VARIANT}' not found under '${OPENSSL_GENERATED}'\n"
        "Available variants come from cmake/patches/openssl_generated_*.zip")
endif ()
include("${OPENSSL_SOURCES}")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Compile-time -D flags OpenSSL's Configure chose for this target (e.g. WIN32_LEAN_AND_MEAN on Windows).
set(OPENSSL_VARIANT_DEFINES "")
set(_defines_file "${OPENSSL_GENERATED}/defines-${OPENSSL_VARIANT}.cmake")
if (EXISTS "${_defines_file}")
    include("${_defines_file}")
endif ()

set(OPENSSL_INCLUDE_DIRS
        "${_variant}/include"
        "${_common}/include"
        "${OPENSSL_SRC}/include"
        "${OPENSSL_SRC}"
        "${OPENSSL_SRC}/crypto"
        "${OPENSSL_SRC}/providers/common/include"
        "${OPENSSL_SRC}/providers/implementations/include"
        "${OPENSSL_SRC}/providers/common/der"
        "${_common}"
        "${_common}/crypto"
        "${_common}/providers/common/include"
        "${_common}/providers/common/der")

set(OPENSSL_OPENSSLDIR "${OPENSSL_INSTALL_DIR}/ssl" CACHE PATH "Compiled-in openssldir")
set(OPENSSL_ENGINESDIR "${OPENSSL_INSTALL_DIR}/lib/engines-3" CACHE PATH "Compiled-in engines dir")
set(OPENSSL_MODULESDIR "${OPENSSL_INSTALL_DIR}/lib/ossl-modules" CACHE PATH "Compiled-in modules dir")

function(sky_openssl_library target output sources)
    add_library(${target} STATIC ${sources})
    set_target_properties(${target} PROPERTIES OUTPUT_NAME ${output})
    if (MSVC)
        # OpenSSL keeps the "lib" prefix on Windows static libraries (libcrypto.lib).
        set_target_properties(${target} PROPERTIES PREFIX "lib")
    endif ()
    target_include_directories(${target} PRIVATE ${OPENSSL_INCLUDE_DIRS})
    target_compile_definitions(${target} PRIVATE
            ${OPENSSL_VARIANT_DEFINES}
            "OPENSSLDIR=\"${OPENSSL_OPENSSLDIR}\""
            "ENGINESDIR=\"${OPENSSL_ENGINESDIR}\""
            "MODULESDIR=\"${OPENSSL_MODULESDIR}\"")
    if (MSVC)
        target_compile_options(${target} PRIVATE /W0)
    else ()
        target_compile_options(${target} PRIVATE -Wno-deprecated-declarations -Wno-unused-parameter)
    endif ()
endfunction()

sky_openssl_library(openssl_crypto crypto "${OPENSSL_CRYPTO_SOURCES}")
sky_openssl_library(openssl_ssl ssl "${OPENSSL_SSL_SOURCES}")
sky_openssl_library(openssl_common common "${OPENSSL_COMMON_SOURCES}")
sky_openssl_library(openssl_default default "${OPENSSL_DEFAULT_SOURCES}")
sky_openssl_library(openssl_legacy legacy "${OPENSSL_LEGACY_SOURCES}")

target_link_libraries(openssl_ssl PUBLIC openssl_crypto)
target_link_libraries(openssl_default PUBLIC openssl_common openssl_crypto)
target_link_libraries(openssl_legacy PUBLIC openssl_crypto)

if (WIN32)
    set(OPENSSL_SYSTEM_LIBS ws2_32 crypt32 advapi32 user32 gdi32 bcrypt)
else ()
    set(OPENSSL_SYSTEM_LIBS pthread dl)
    if (ANDROID)
        list(APPEND OPENSSL_SYSTEM_LIBS log)
    endif ()
endif ()
foreach (_lib crypto ssl common default legacy)
    target_link_libraries(openssl_${_lib} PUBLIC ${OPENSSL_SYSTEM_LIBS})
endforeach ()

if (OPENSSL_INSTALL_DIR)
    install(TARGETS openssl_crypto openssl_ssl openssl_common openssl_default openssl_legacy
            ARCHIVE DESTINATION lib)
    install(DIRECTORY "${_variant}/include/openssl/" DESTINATION include/openssl)
    install(DIRECTORY "${_common}/include/openssl/" DESTINATION include/openssl)
    install(DIRECTORY "${OPENSSL_SRC}/include/openssl/" DESTINATION include/openssl)
endif ()
