# Helper functions to reduce boilerplate in Find*.cmake modules.
# Include this file BEFORE any find_package() calls.

# Creates an INTERFACE IMPORTED target for header-only libraries.
# Usage:
#   sky_3rd_header_only(<name> [INCLUDE_SUBDIR <subdir>])
# Example:
#   sky_3rd_header_only(rapidjson)
#   sky_3rd_header_only(freetype INCLUDE_SUBDIR freetype2)
function(sky_3rd_header_only LIB_NAME)
    cmake_parse_arguments(ARGS "" "INCLUDE_SUBDIR" "" ${ARGN})
    set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")
    if (TARGET ${TARGET_WITH_NAMESPACE})
        set(${LIB_NAME}_FOUND True PARENT_SCOPE)
        return()
    endif()

    if (ARGS_INCLUDE_SUBDIR)
        set(INC_DIR ${${LIB_NAME}_PATH}/include/${ARGS_INCLUDE_SUBDIR})
    else()
        set(INC_DIR ${${LIB_NAME}_PATH}/include)
    endif()

    add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
    target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${INC_DIR})
    set(${LIB_NAME}_FOUND True PARENT_SCOPE)
endfunction()

# Creates an INTERFACE IMPORTED target for prebuilt static libraries.
# Handles Debug/Release paths and optional debug postfix.
# Usage:
#   sky_3rd_static(<name> LIBS <lib1> [lib2 ...]
#                  [DEBUG_SUFFIX <suffix>]
#                  [INCLUDE_SUBDIR <subdir>]
#                  [EXT_LIBS <lib> ...])
# Examples:
#   sky_3rd_static(crc32 LIBS crc32c)
#   sky_3rd_static(bullet3 INCLUDE_SUBDIR bullet DEBUG_SUFFIX d
#       LIBS Bullet3Collision Bullet3Common Bullet3Dynamics
#            Bullet3Geometry BulletCollision BulletDynamics LinearMath)
function(sky_3rd_static LIB_NAME)
    cmake_parse_arguments(ARGS "" "DEBUG_SUFFIX;INCLUDE_SUBDIR" "LIBS;EXT_LIBS" ${ARGN})
    set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")
    if (TARGET ${TARGET_WITH_NAMESPACE})
        set(${LIB_NAME}_FOUND True PARENT_SCOPE)
        return()
    endif()

    if (ARGS_INCLUDE_SUBDIR)
        set(INC_DIR ${${LIB_NAME}_PATH}/include/${ARGS_INCLUDE_SUBDIR})
    else()
        set(INC_DIR ${${LIB_NAME}_PATH}/include)
    endif()

    set(LIBS_DIR ${${LIB_NAME}_PATH}/lib)

    foreach(lib ${ARGS_LIBS})
        list(APPEND DEBUG_LIBS
            ${LIBS_DIR}/Debug/${CMAKE_STATIC_LIBRARY_PREFIX}${lib}${ARGS_DEBUG_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX})
        list(APPEND RELEASE_LIBS
            ${LIBS_DIR}/Release/${CMAKE_STATIC_LIBRARY_PREFIX}${lib}${CMAKE_STATIC_LIBRARY_SUFFIX})
    endforeach()

    if (ARGS_EXT_LIBS)
        list(APPEND DEBUG_LIBS ${ARGS_EXT_LIBS})
        list(APPEND RELEASE_LIBS ${ARGS_EXT_LIBS})
    endif()

    set(LIBRARY
        "$<$<CONFIG:debug>:${DEBUG_LIBS}>"
        "$<$<CONFIG:release>:${RELEASE_LIBS}>")

    add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
    target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${INC_DIR})
    target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE ${LIBRARY})
    set(${LIB_NAME}_FOUND True PARENT_SCOPE)
endfunction()
