set(LIB_NAME "slang")
set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")

set(${LIB_NAME}_INCLUDE_DIR ${${LIB_NAME}_PATH}/include)

if (WIN32)
    set(${LIB_NAME}_LIBRARY_DEBUG
        ${${LIB_NAME}_PATH}/lib/Debug/slang${CMAKE_STATIC_LIBRARY_SUFFIX})
    set(${LIB_NAME}_LIBRARY_RELEASE
        ${${LIB_NAME}_PATH}/lib/Release/slang${CMAKE_STATIC_LIBRARY_SUFFIX})
    set(${LIB_NAME}_LIBRARY
        "$<$<CONFIG:Debug>:${${LIB_NAME}_LIBRARY_DEBUG}>"
        "$<$<CONFIG:Release>:${${LIB_NAME}_LIBRARY_RELEASE}>")

    # slang.dll loads its sibling modules at runtime; copy them all
    file(GLOB ${LIB_NAME}_DLLS ${${LIB_NAME}_PATH}/bin/*.dll)
    set(${LIB_NAME}_DYNAMIC_LIBRARY ${${LIB_NAME}_DLLS})
elseif (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # iOS: slang's shared lib is not produced by its iOS/Xcode build, so it is
    # built as static libraries. The main lib is not self-contained, so link all
    # static libs shipped in the package (core, compiler-core, slang-cpp-parser,
    # slang-compiler, ...).
    file(GLOB ${LIB_NAME}_STATIC_LIBS_DEBUG ${${LIB_NAME}_PATH}/lib/Debug/*.a)
    file(GLOB ${LIB_NAME}_STATIC_LIBS_RELEASE ${${LIB_NAME}_PATH}/lib/Release/*.a)
    set(${LIB_NAME}_LIBRARY
        "$<$<CONFIG:Debug>:${${LIB_NAME}_STATIC_LIBS_DEBUG}>"
        "$<$<CONFIG:Release>:${${LIB_NAME}_STATIC_LIBS_RELEASE}>")
    set(${LIB_NAME}_DYNAMIC_LIBRARY)
else ()
    set(${LIB_NAME}_LIBRARY_DEBUG
        ${${LIB_NAME}_PATH}/lib/Debug/libslang${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(${LIB_NAME}_LIBRARY_RELEASE
        ${${LIB_NAME}_PATH}/lib/Release/libslang${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(${LIB_NAME}_LIBRARY
        "$<$<CONFIG:Debug>:${${LIB_NAME}_LIBRARY_DEBUG}>"
        "$<$<CONFIG:Release>:${${LIB_NAME}_LIBRARY_RELEASE}>")
    set(${LIB_NAME}_DYNAMIC_LIBRARY ${${LIB_NAME}_LIBRARY_RELEASE})
endif ()

add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_INCLUDE_DIR})
target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_LIBRARY})
set_target_properties(${TARGET_WITH_NAMESPACE} PROPERTIES INTERFACE_DYN_LIBS "${${LIB_NAME}_DYNAMIC_LIBRARY}")

set(${LIB_NAME}_FOUND True)
