
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