add_library(Aurora.Renderer.Interface INTERFACE)
target_compile_features(Aurora.Renderer.Interface INTERFACE cxx_std_20)
target_include_directories(Aurora.Renderer.Interface INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(Aurora.Renderer.Interface INTERFACE Core Aurora.RHI.Interface)
install(TARGETS Aurora.Renderer.Interface EXPORT SkyEngineExport)

file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

sky_add_library(TARGET Aurora.Core.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
        src/renderer
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        Aurora.RHI.Static
        3rdParty::rapidjson
    INSTALL_DIR
        include
)

file(GLOB TEST_FILES test/*.cpp)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Aurora.CoreTest
        SOURCES
            ${TEST_FILES}
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test/main.cpp
        INCS
            ${CMAKE_CURRENT_SOURCE_DIR}/../rhi/test
        LIBS
            Aurora.Core.Static
            Aurora.RHI.Static
            3rdParty::googletest
    )
    target_compile_definitions(Aurora.CoreTest PRIVATE
        SHADER_TEST_PACKAGE_ROOT="${PROJECT_BINARY_DIR}/shader-package-test/relocated/shaders")
endif ()
