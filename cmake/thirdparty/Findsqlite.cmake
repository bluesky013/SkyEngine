set(LIB_NAME "sqlite")
set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")

if (MSVC)
    set(${LIB_NAME}_DYNAMIC_LIBRARY ${${LIB_NAME}_PATH}/sqlite3.dll)
elseif (APPLE)
    set(${LIB_NAME}_DYNAMIC_LIBRARY ${${LIB_NAME}_PATH}/libsqlite3.dylib)
endif()

set(${LIB_NAME}_INCLUDE_DIR ${${LIB_NAME}_PATH}/include)

add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_INCLUDE_DIR})
set_target_properties(${TARGET_WITH_NAMESPACE} PROPERTIES DYN_LIBS ${${LIB_NAME}_DYNAMIC_LIBRARY})

set(${LIB_NAME}_FOUND True)
