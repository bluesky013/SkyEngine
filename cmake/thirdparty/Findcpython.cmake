set(LIB_NAME "cpython")
set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")

set(${LIB_NAME}_INCLUDE_DIR ${${LIB_NAME}_PATH}/include)
set(${LIB_NAME}_LIBS_DIR ${${LIB_NAME}_PATH}/lib)

if (MSVC)
    set(${LIB_NAME}_DYNAMIC_LIBRARY ${${LIB_NAME}_PATH}/bin/Release/python313.dll)
    set(${LIB_NAME}_LIBRARY_DEBUG ${${LIB_NAME}_LIBS_DIR}/Debug/python313_d.lib)
    set(${LIB_NAME}_LIBRARY_RELEASE ${${LIB_NAME}_LIBS_DIR}/Release/python313.lib)
elseif (APPLE)
    set(${LIB_NAME}_DYNAMIC_LIBRARY ${${LIB_NAME}_PATH}/python313.dylib)
else()
    set(${LIB_NAME}_DYNAMIC_LIBRARY ${${LIB_NAME}_PATH}/python313.so)
endif()

set(${LIB_NAME}_LIBRARY
        "$<$<CONFIG:release>:${${LIB_NAME}_LIBRARY_RELEASE}>"
        "$<$<CONFIG:debug>:${${LIB_NAME}_LIBRARY_DEBUG}>")

add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_INCLUDE_DIR})
target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_LIBRARY})
set_target_properties(${TARGET_WITH_NAMESPACE} PROPERTIES INTERFACE_DYN_LIBS ${${LIB_NAME}_DYNAMIC_LIBRARY})

set(${LIB_NAME}_FOUND True)
