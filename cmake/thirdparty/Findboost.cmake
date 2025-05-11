set(LIB_NAME "boost")
set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")

set(${LIB_NAME}_INCLUDE_DIR ${${LIB_NAME}_PATH}/include)
set(${LIB_NAME}_LIBS_DIR ${${LIB_NAME}_PATH}/lib)

set(BOOST_LIB_NAMES
        container
        graph
)

foreach(lib ${BOOST_LIB_NAMES})
    set(${LIB_NAME}_LIBRARY_DEBUG ${${LIB_NAME}_LIBRARY_DEBUG}
            ${${LIB_NAME}_LIBS_DIR}/Debug/${CMAKE_STATIC_LIBRARY_PREFIX}libboost_${lib}${CMAKE_STATIC_LIBRARY_SUFFIX})

    set(${LIB_NAME}_LIBRARY_RELEASE ${${LIB_NAME}_LIBRARY_RELEASE}
            ${${LIB_NAME}_LIBS_DIR}/Release/${CMAKE_STATIC_LIBRARY_PREFIX}libboost_${lib}${CMAKE_STATIC_LIBRARY_SUFFIX})
endforeach()

set(${LIB_NAME}_LIBRARY
        "$<$<CONFIG:release>:${${LIB_NAME}_LIBRARY_RELEASE}>"
        "$<$<CONFIG:debug>:${${LIB_NAME}_LIBRARY_DEBUG}>")

add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_INCLUDE_DIR})
target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_LIBRARY})

set(${LIB_NAME}_FOUND True)
