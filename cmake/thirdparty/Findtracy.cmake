set(LIB_NAME "tracy")
set(TARGET_WITH_NAMESPACE        "3rdParty::${LIB_NAME}")
if (TARGET ${TARGET_WITH_NAMESPACE})
    return()
endif()

set(${LIB_NAME}_INCLUDE_DIR ${${LIB_NAME}_PATH}/include)
set(${LIB_NAME}_LIBS_DIR ${${LIB_NAME}_PATH}/lib)
set(${LIB_NAME}_BIN_DIR ${${LIB_NAME}_PATH}/bin)

set(${LIB_NAME}_LIBRARY_DEBUG
        ${${LIB_NAME}_LIBS_DIR}/Debug/${CMAKE_STATIC_LIBRARY_PREFIX}TracyClient${CMAKE_STATIC_LIBRARY_SUFFIX})

set(${LIB_NAME}_LIBRARY_RELEASE
        ${${LIB_NAME}_LIBS_DIR}/Release/${CMAKE_STATIC_LIBRARY_PREFIX}TracyClient${CMAKE_STATIC_LIBRARY_SUFFIX})

#set(${LIB_NAME}_DYN_LIBRARY_DEBUG
#        ${${LIB_NAME}_BIN_DIR}/Debug/TracyClient.dll)

set(${LIB_NAME}_DYN_LIBRARY_RELEASE
        ${${LIB_NAME}_BIN_DIR}/Release/TracyClient.dll)

set(${LIB_NAME}_LIBRARY
        "$<$<CONFIG:release>:${${LIB_NAME}_LIBRARY_RELEASE}>"
        "$<$<CONFIG:debug>:${${LIB_NAME}_LIBRARY_DEBUG}>")

add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_INCLUDE_DIR})
target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_LIBRARY})

set_target_properties(${TARGET_WITH_NAMESPACE} PROPERTIES INTERFACE_DYN_LIBS ${${LIB_NAME}_DYN_LIBRARY_RELEASE})

set(${LIB_NAME}_FOUND True)
