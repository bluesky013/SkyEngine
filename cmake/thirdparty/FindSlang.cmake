set(LIB_NAME "slang")
set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")

set(${LIB_NAME}_INCLUDE_DIR ${${LIB_NAME}_PATH}/include)

# Slang is built as a static library on every platform and linked into
# Aurora.ShaderCompiler. The main archive is not self-contained, so link every
# archive shipped for the configuration (core, compiler-core, parser, lz4, ...).
set(_slang_static_suffix ${CMAKE_STATIC_LIBRARY_SUFFIX})

# Static linkers resolve archives left to right; keep dependents before their
# dependencies (slang-compiler -> compiler-core -> parser -> core -> bundled).
function(_slang_order_archives out)
    set(_archives ${ARGN})
    set(_ordered)
    foreach (_name slang-compiler compiler-core slang-cpp-parser core cmark-gfm lz4 miniz)
        set(_matches)
        foreach (_archive IN LISTS _archives)
            get_filename_component(_stem "${_archive}" NAME_WE)
            string(REGEX REPLACE "^lib" "" _stem "${_stem}")
            if (_stem STREQUAL _name OR _stem MATCHES "^${_name}[-.]")
                list(APPEND _matches "${_archive}")
            endif ()
        endforeach ()
        if (_matches)
            list(APPEND _ordered ${_matches})
            list(REMOVE_ITEM _archives ${_matches})
        endif ()
    endforeach ()
    list(APPEND _ordered ${_archives})
    set(${out} "${_ordered}" PARENT_SCOPE)
endfunction()

file(GLOB ${LIB_NAME}_STATIC_LIBS_DEBUG ${${LIB_NAME}_PATH}/lib/Debug/*${_slang_static_suffix})
file(GLOB ${LIB_NAME}_STATIC_LIBS_RELEASE ${${LIB_NAME}_PATH}/lib/Release/*${_slang_static_suffix})
_slang_order_archives(${LIB_NAME}_STATIC_LIBS_DEBUG ${${LIB_NAME}_STATIC_LIBS_DEBUG})
_slang_order_archives(${LIB_NAME}_STATIC_LIBS_RELEASE ${${LIB_NAME}_STATIC_LIBS_RELEASE})

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
    find_package(Threads REQUIRED)
    target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE ${CMAKE_DL_LIBS} Threads::Threads)
endif ()
set_target_properties(${TARGET_WITH_NAMESPACE} PROPERTIES INTERFACE_DYN_LIBS "${${LIB_NAME}_DYNAMIC_LIBRARY}")

set(${LIB_NAME}_FOUND True)
