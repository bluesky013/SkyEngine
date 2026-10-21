sky_add_test(TARGET Aurora.Renderer.ContractTest
    SOURCES RendererContractTest.cpp
    LIBS Aurora.Renderer.Interface
    WORKING_DIR $<TARGET_FILE_DIR:Aurora.Renderer.ContractTest>)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Aurora.Renderer.ProductsTest
        SOURCES PrepareProductsTest.cpp
        INCS ../../src/renderer
        LIBS Aurora.Core.Static 3rdParty::googletest
        WORKING_DIR $<TARGET_FILE_DIR:Aurora.Renderer.ProductsTest>)
    sky_add_test(TARGET Aurora.Renderer.ChunkPrepareTest
        SOURCES ChunkPrepareTest.cpp
        INCS ../../src/renderer
        LIBS Aurora.Core.Static 3rdParty::googletest
        WORKING_DIR $<TARGET_FILE_DIR:Aurora.Renderer.ChunkPrepareTest>)
    sky_add_test(TARGET Aurora.Renderer.ParallelPrepareTest
        SOURCES ParallelPrepareTest.cpp
        INCS ../../src/renderer
        LIBS Aurora.Core.Static 3rdParty::googletest
        WORKING_DIR $<TARGET_FILE_DIR:Aurora.Renderer.ParallelPrepareTest>)
    sky_add_test(TARGET Aurora.Renderer.WriteViewTest
        SOURCES RendererWriteViewTest.cpp
        INCS ../../src/renderer
        LIBS Aurora.Core.Static 3rdParty::googletest
        WORKING_DIR $<TARGET_FILE_DIR:Aurora.Renderer.WriteViewTest>)
    sky_add_test(TARGET Aurora.Renderer.PrepareTest
        SOURCES SerialPrepareTest.cpp
        INCS ../../src/renderer
        LIBS Aurora.Core.Static 3rdParty::googletest
        WORKING_DIR $<TARGET_FILE_DIR:Aurora.Renderer.PrepareTest>)
    sky_add_test(TARGET Aurora.Renderer.StateTest
        SOURCES RendererStateTest.cpp
        INCS ../../src/renderer
        LIBS Aurora.Core.Static 3rdParty::googletest
        WORKING_DIR $<TARGET_FILE_DIR:Aurora.Renderer.StateTest>)
    sky_add_test(TARGET Aurora.Renderer.CoordinatorTest
        SOURCES RendererCoordinatorTest.cpp
        INCS ../../src/renderer
        LIBS Aurora.Core.Static 3rdParty::googletest
        WORKING_DIR $<TARGET_FILE_DIR:Aurora.Renderer.CoordinatorTest>)
endif()

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Aurora.Renderer.ResourceTest
        SOURCES ResourceCatalogTest.cpp PreparedSceneTest.cpp
        INCS ../../src/renderer
        LIBS Aurora.Core.Static 3rdParty::googletest
        WORKING_DIR $<TARGET_FILE_DIR:Aurora.Renderer.ResourceTest>)
endif()

if (SKY_BUILD_TEST AND WIN32)
    sky_add_test(TARGET Aurora.Renderer.GpuSubmissionTest
        SOURCES EditorHostNativeTest.cpp RendererHostNativeTest.cpp RendererRuntimeTest.cpp FrameSchedulerTest.cpp FrameResourceStateTest.cpp GpuSubmissionTest.cpp FrameContextTest.cpp FrameGraphTest.cpp VulkanFrameCompilerTest.cpp D3D12FrameCompilerTest.cpp FrameAllocationTest.cpp ResourceUploadTest.cpp IndexedRasterTest.cpp ImageSubresourceTest.cpp FrameTransientTest.cpp
        INCS ../../src/renderer
        LIBS EditorRender Aurora.Core.Static Aurora.Adaptor.Static Aurora.RHI.Static Aurora.FrameBackend.Static Aurora.Vulkan Aurora.DX12 Aurora.ShaderCompiler.Static 3rdParty::googletest 3rdParty::vulkan_headers 3rdParty::vma 3rdParty::d3d12ma d3dcompiler
        WORKING_DIR $<TARGET_FILE_DIR:Aurora.Renderer.GpuSubmissionTest>)
    target_include_directories(Aurora.Renderer.GpuSubmissionTest PRIVATE
        ${PROJECT_SOURCE_DIR}/engine/aurora/pipeline/shaders
        ${PROJECT_SOURCE_DIR}/engine/aurora/rhi/vulkan/include
        ${PROJECT_SOURCE_DIR}/engine/aurora/rhi/vulkan/src
        ${PROJECT_SOURCE_DIR}/engine/aurora/rhi/dx12/include
        ${PROJECT_SOURCE_DIR}/engine/aurora/rhi/dx12/src)
    target_compile_definitions(Aurora.Renderer.GpuSubmissionTest PRIVATE VK_NO_PROTOTYPES VK_USE_PLATFORM_WIN32_KHR)
    add_dependencies(Aurora.Renderer.GpuSubmissionTest Aurora.Vulkan Aurora.DX12)
    target_compile_definitions(Aurora.Renderer.GpuSubmissionTest PRIVATE
        AURORA_SHARED_SHADER_DIR="${PROJECT_SOURCE_DIR}/engine/aurora/pipeline/shaders"
        AURORA_SHADER_INCLUDE_DIR="${PROJECT_SOURCE_DIR}/engine/aurora/core/include")
    sky_set_dependency(TARGET Aurora.Renderer.GpuSubmissionTest)
endif()
