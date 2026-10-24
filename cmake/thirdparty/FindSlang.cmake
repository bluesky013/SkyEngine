set(LIB_NAME "slang")
set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")

set(${LIB_NAME}_INCLUDE_DIR ${${LIB_NAME}_PATH}/include)

# Slang is built as a static library on every platform and linked into
# Aurora.ShaderCompiler. The main archive is not self-contained, so link every
# archive shipped for the configuration (core, compiler-core, parser, lz4, ...).
if (WIN32)
    set(_slang_static_suffix ".lib")
else ()
    set(_slang_static_suffix ".a")
endif ()

file(GLOB ${LIB_NAME}_STATIC_LIBS_DEBUG ${${LIB_NAME}_PATH}/lib/Debug/*${_slang_static_suffix})
file(GLOB ${LIB_NAME}_STATIC_LIBS_RELEASE ${${LIB_NAME}_PATH}/lib/Release/*${_slang_static_suffix})
set(${LIB_NAME}_LIBRARY)
foreach (_library IN LISTS ${LIB_NAME}_STATIC_LIBS_DEBUG)
    list(APPEND ${LIB_NAME}_LIBRARY "$<$<CONFIG:Debug>:${_library}>")
endforeach ()
foreach (_library IN LISTS ${LIB_NAME}_STATIC_LIBS_RELEASE)
    list(APPEND ${LIB_NAME}_LIBRARY "$<$<NOT:$<CONFIG:Debug>>:${_library}>")
endforeach ()
set(${LIB_NAME}_DYNAMIC_LIBRARY)

add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_INCLUDE_DIR})
target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_LIBRARY})
if (UNIX)
    target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE ${CMAKE_DL_LIBS})
    if (TARGET Threads::Threads)
        target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE Threads::Threads)
    endif ()
endif ()
set_target_properties(${TARGET_WITH_NAMESPACE} PROPERTIES INTERFACE_DYN_LIBS "${${LIB_NAME}_DYNAMIC_LIBRARY}")

set(${LIB_NAME}_FOUND True)
