set(LIB_NAME "ispc_texcomp")
set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")

if (MSVC)
    set(${LIB_NAME}_DYNAMIC_LIBRARY ${${LIB_NAME}_PATH}/ispc_texcomp.dll)
elseif (APPLE)
    set(${LIB_NAME}_DYNAMIC_LIBRARY ${${LIB_NAME}_PATH}/libispc_texcomp.dylib)
endif()

set(${LIB_NAME}_INCLUDE_DIR ${${LIB_NAME}_PATH}/include)

add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_INCLUDE_DIR})
set_target_properties(${TARGET_WITH_NAMESPACE} PROPERTIES INTERFACE_DYN_LIBS ${${LIB_NAME}_DYNAMIC_LIBRARY})

set(${LIB_NAME}_FOUND True)
