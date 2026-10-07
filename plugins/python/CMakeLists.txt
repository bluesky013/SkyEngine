file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

include(${ENGINE_ROOT}/cmake/plugin_config.cmake)

# The cpython package records whether it was built with _ssl/_hashlib (ssl.txt).
# Build-time is the source of truth: whenever the core embeds ssl, pull OpenSSL so
# Findcpython links it — regardless of the SKY_PYTHON_SSL configure hint.
set(_python_ssl OFF)
if (SKY_PYTHON_SSL OR EXISTS "${3RD_PATH}/cpython/ssl.txt")
    set(_python_ssl ON)
endif ()

set(_python_ssl_defines "")
if (_python_ssl)
    sky_find_3rd(TARGET openssl DIR openssl)
    list(APPEND _python_ssl_defines SKY_PYTHON_SSL)
endif ()

sky_find_3rd(TARGET cpython DIR cpython)

sky_add_library(TARGET PythonModule SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        PythonModule.cpp
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    PRIVATE_DEFS
        ${_python_ssl_defines}
    LINK_LIBS
        Framework
        3rdParty::cpython
)

sky_plugin_apply_target_dependencies(CONFIG "${CMAKE_CURRENT_LIST_DIR}/plugin.json")

if (SKY_BUILD_TEST)
    sky_add_test(TARGET PythonRuntimeTest
        SOURCES
            ${CMAKE_CURRENT_LIST_DIR}/test/PythonRuntimeTest.cpp
        WORKING_DIR
            ${CMAKE_SOURCE_DIR}
        LIBS
            PythonModule
            Framework
            3rdParty::googletest
    )

    if (_python_ssl)
        target_compile_definitions(PythonRuntimeTest PRIVATE SKY_PYTHON_SSL)
    endif ()

    set(_python_test_env "SKY_PYTHON_HOME=${3RD_PATH}/cpython")
    if (_python_ssl)
        list(APPEND _python_test_env "SKY_PYTHON_SSL_CERT=${3RD_PATH}/openssl/ssl/cert.pem")
    endif ()

    set_tests_properties(PythonRuntimeTest PROPERTIES
        ENVIRONMENT "${_python_test_env}")
endif ()