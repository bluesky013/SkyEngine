set(LIB_NAME "lz4")
set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")
if (TARGET ${TARGET_WITH_NAMESPACE})
    return()
endif()

set(${LIB_NAME}_INCLUDE_DIR ${${LIB_NAME}_PATH}/include)
set(${LIB_NAME}_LIBS_DIR ${${LIB_NAME}_PATH}/lib)

set(${LIB_NAME}_LIBRARY_DEBUG
        ${${LIB_NAME}_LIBS_DIR}/Debug/${CMAKE_STATIC_LIBRARY_PREFIX}lz4${CMAKE_STATIC_LIBRARY_SUFFIX})

set(${LIB_NAME}_LIBRARY_RELEASE
        ${${LIB_NAME}_LIBS_DIR}/Release/${CMAKE_STATIC_LIBRARY_PREFIX}lz4${CMAKE_STATIC_LIBRARY_SUFFIX})

set(${LIB_NAME}_LIBRARY
        "$<$<CONFIG:release>:${${LIB_NAME}_LIBRARY_RELEASE}>"
        "$<$<CONFIG:debug>:${${LIB_NAME}_LIBRARY_DEBUG}>")

add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_INCLUDE_DIR})
target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_LIBRARY})

set(${LIB_NAME}_FOUND True)
