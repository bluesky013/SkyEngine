set(LIB_NAME "bullet3")
set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")
set(${LIB_NAME}_INCLUDE_DIR ${${LIB_NAME}_PATH}/include/bullet)
set(${LIB_NAME}_LIBS_DIR ${${LIB_NAME}_PATH}/lib)

if (MSVC)
set(LIB_SUFFIX "d")
endif ()

set(${LIB_NAME}_LIBRARY_DEBUG
        ${${LIB_NAME}_LIBS_DIR}/Debug/${CMAKE_STATIC_LIBRARY_PREFIX}Bullet3Collision${LIB_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${${LIB_NAME}_LIBS_DIR}/Debug/${CMAKE_STATIC_LIBRARY_PREFIX}Bullet3Common${LIB_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${${LIB_NAME}_LIBS_DIR}/Debug/${CMAKE_STATIC_LIBRARY_PREFIX}Bullet3Dynamics${LIB_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${${LIB_NAME}_LIBS_DIR}/Debug/${CMAKE_STATIC_LIBRARY_PREFIX}Bullet3Geometry${LIB_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${${LIB_NAME}_LIBS_DIR}/Debug/${CMAKE_STATIC_LIBRARY_PREFIX}BulletCollision${LIB_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${${LIB_NAME}_LIBS_DIR}/Debug/${CMAKE_STATIC_LIBRARY_PREFIX}BulletDynamics${LIB_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${${LIB_NAME}_LIBS_DIR}/Debug/${CMAKE_STATIC_LIBRARY_PREFIX}LinearMath${LIB_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}
)

set(${LIB_NAME}_LIBRARY_RELEASE
        ${${LIB_NAME}_LIBS_DIR}/Release/${CMAKE_STATIC_LIBRARY_PREFIX}Bullet3Collision${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${${LIB_NAME}_LIBS_DIR}/Release/${CMAKE_STATIC_LIBRARY_PREFIX}Bullet3Common${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${${LIB_NAME}_LIBS_DIR}/Release/${CMAKE_STATIC_LIBRARY_PREFIX}Bullet3Dynamics${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${${LIB_NAME}_LIBS_DIR}/Release/${CMAKE_STATIC_LIBRARY_PREFIX}Bullet3Geometry${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${${LIB_NAME}_LIBS_DIR}/Release/${CMAKE_STATIC_LIBRARY_PREFIX}BulletCollision${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${${LIB_NAME}_LIBS_DIR}/Release/${CMAKE_STATIC_LIBRARY_PREFIX}BulletDynamics${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${${LIB_NAME}_LIBS_DIR}/Release/${CMAKE_STATIC_LIBRARY_PREFIX}LinearMath${CMAKE_STATIC_LIBRARY_SUFFIX}
)

set(${LIB_NAME}_LIBRARY
        "$<$<CONFIG:release>:${${LIB_NAME}_LIBRARY_RELEASE}>"
        "$<$<CONFIG:debug>:${${LIB_NAME}_LIBRARY_DEBUG}>")

add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_INCLUDE_DIR})
target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_LIBRARY})

set(${LIB_NAME}_FOUND True)
