set(LIB_NAME "OpenXR")
set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")
if (TARGET ${TARGET_WITH_NAMESPACE})
    return()
endif()

set(${LIB_NAME}_INCLUDE_DIR ${${LIB_NAME}_PATH}/include)
set(${LIB_NAME}_LIBS_DIR ${${LIB_NAME}_PATH}/lib)

if (MSVC)
    set(DEBUG_SUFFIX "d")
endif()

if (ANDROID)
    set(${LIB_NAME}_DYNAMIC_LIBRARY ${${LIB_NAME}_PATH}/libopenxr_loader.so)
else ()
    set(${LIB_NAME}_LIBRARY_DEBUG
            ${${LIB_NAME}_LIBS_DIR}/Debug/${CMAKE_STATIC_LIBRARY_PREFIX}openxr_loader${DEBUG_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX})

    set(${LIB_NAME}_LIBRARY_RELEASE
            ${${LIB_NAME}_LIBS_DIR}/Release/${CMAKE_STATIC_LIBRARY_PREFIX}openxr_loader${CMAKE_STATIC_LIBRARY_SUFFIX})

    set(${LIB_NAME}_LIBRARY
            "$<$<CONFIG:release>:${${LIB_NAME}_LIBRARY_RELEASE}>"
            "$<$<CONFIG:debug>:${${LIB_NAME}_LIBRARY_DEBUG}>")
endif ()

add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_INCLUDE_DIR})
target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_LIBRARY})

set(${LIB_NAME}_FOUND True)
