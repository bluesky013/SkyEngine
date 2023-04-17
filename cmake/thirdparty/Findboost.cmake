set(LIB_NAME "boost")
set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")

set(${LIB_NAME}_INCLUDE_DIR ${${LIB_NAME}_PATH})
set(${LIB_NAME}_ROOT_DIR ${${LIB_NAME}_PATH})

add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_INCLUDE_DIR})

set(BOOST_LIB_NAMES
    container
    )

foreach(lib ${BOOST_LIB_NAMES})
    target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_PATH}/stage/lib/libboost_${lib}.a)
endforeach()

set(${LIB_NAME}_FOUND True)
