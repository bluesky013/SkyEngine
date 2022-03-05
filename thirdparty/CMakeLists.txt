
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(IMGUI_SRC
    ${IMGUI_DIR}/imgui.h
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
)
sky_add_library(TARGET Imgui STATIC
    SOURCES
        ${IMGUI_SRC}
    PUBLIC_INC
        ${IMGUI_DIR}
)

set(VMA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator)
add_library(VMA INTERFACE IMPORTED GLOBAL)
target_include_directories(VMA INTERFACE ${VMA_DIR}/src)

set(GLM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/glm)
add_library(GLM INTERFACE IMPORTED GLOBAL)
target_include_directories(GLM INTERFACE ${GLM_DIR})

add_library(googletest STATIC
    googletest/googletest/src/gtest-all.cc
    )
target_include_directories(googletest PUBLIC googletest/googletest/include)
target_include_directories(googletest PRIVATE googletest/googletest)

add_library(sfmt STATIC
    SFMT-src-1.5.1/SFMT.c
    )
target_compile_definitions(sfmt PUBLIC -DSFMT_MEXP=19937)
target_include_directories(sfmt PUBLIC SFMT-src-1.5.1)

set(TINYGLTF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tinygltf)
add_library(Tinygltf INTERFACE IMPORTED GLOBAL)
target_include_directories(Tinygltf INTERFACE ${TINYGLTF_DIR})

set(RAPIDJSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rapidjson)
add_library(Rapidjson INTERFACE IMPORTED GLOBAL)
target_include_directories(Rapidjson INTERFACE ${RAPIDJSON_DIR}/include)

set(TASKFLOW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/taskflow)
add_library(TaskFlow INTERFACE IMPORTED GLOBAL)
target_include_directories(TaskFlow INTERFACE ${TASKFLOW_DIR}/taskflow)


set(CRC32C_BUILD_BENCHMARKS false)
set(CRC32C_BUILD_TESTS false)
set(CRC32C_INSTALL false)
set(CRC32C_USE_GLOG false)
set(gtest_force_shared_crt false)
add_subdirectory(crc32c)

set(BUILD_TESTING false)
set(ENABLE_CTEST false)
add_subdirectory(glslang)

set(ASSIMP_BUILD_TESTS false)
set(BUILD_SHARED_LIBS false)
add_subdirectory(assimp)

set(SPIRV_CROSS_CLI false)
set(SPIRV_CROSS_ENABLE_TESTS false)
add_subdirectory(SPIRV-Cross)

set(CEREAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cereal)
add_library(cereal INTERFACE IMPORTED GLOBAL)
target_include_directories(cereal INTERFACE ${CEREAL_DIR}/include)