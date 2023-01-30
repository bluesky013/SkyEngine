set(LIB_NAME "gles")
set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")

if (WIN32)
    set(${LIB_NAME}_DYNAMIC_LIBRARY
        ${${LIB_NAME}_PATH}/bin/libEGL.dll
        ${${LIB_NAME}_PATH}/bin/libGLESv2.dll)

    set(${LIB_NAME}_STATIC_LIBRARY
        ${${LIB_NAME}_PATH}/lib/glad.lib)
endif()

set(${LIB_NAME}_INCLUDE_DIR ${${LIB_NAME}_PATH}/include)

add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_INCLUDE_DIR})
target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_STATIC_LIBRARY})
set_property(TARGET ${TARGET_WITH_NAMESPACE} PROPERTY DYN_LIBS ${${LIB_NAME}_DYNAMIC_LIBRARY})

set(${LIB_NAME}_FOUND True)
