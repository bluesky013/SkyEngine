set(LIB_NAME "volk")
set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")
set(${LIB_NAME}_INCLUDE_DIR ${${LIB_NAME}_PATH}/include)

add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_INCLUDE_DIR})
set(${LIB_NAME}_FOUND True)
