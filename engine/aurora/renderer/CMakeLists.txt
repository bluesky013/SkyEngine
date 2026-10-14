add_library(Aurora.Renderer.Interface INTERFACE)
target_compile_features(Aurora.Renderer.Interface INTERFACE cxx_std_20)
target_include_directories(Aurora.Renderer.Interface INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(Aurora.Renderer.Interface INTERFACE Core Aurora.RHI.Interface)
install(TARGETS Aurora.Renderer.Interface EXPORT SkyEngineExport)
install(DIRECTORY include/ DESTINATION include FILES_MATCHING PATTERN "*.h")

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Aurora.Renderer.ContractTest
        SOURCES test/RendererContractTest.cpp
        LIBS Aurora.Renderer.Interface
        WORKING_DIR $<TARGET_FILE_DIR:Aurora.Renderer.ContractTest>)
endif()
