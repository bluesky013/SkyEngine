# Process metadata and runtime renderer entries reuse the existing adaptor implementation.
sky_add_module(TARGET Aurora.Metadata
    SOURCES src/AuroraMetadata.cpp
    LINK_LIBS Aurora.Adaptor.Static Framework
)
sky_add_dependency(TARGET Aurora.Metadata DEPENDENCIES Launcher SandboxEditor AssetTool)

sky_add_module(TARGET Aurora.Render
    SOURCES
        src/AuroraRegistry.cpp
    LINK_LIBS
        Aurora.Adaptor.Static
        Core
)

# backends are dlopen'd by Instance at runtime; only need them built + deployed
if (TARGET Aurora.Vulkan)
    add_dependencies(Aurora.Render Aurora.Vulkan)
endif()
if (TARGET Aurora.DX12)
    add_dependencies(Aurora.Render Aurora.DX12)
endif()
if (TARGET Aurora.Metal)
    add_dependencies(Aurora.Render Aurora.Metal)
endif()

# build Aurora.Render before the launcher so its dll lands next to the exe
sky_add_dependency(TARGET Aurora.Render DEPENDENCIES Launcher SandboxEditor)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Aurora.Renderer.ModuleTest
        SOURCES ../core/test/renderer/RendererModuleTest.cpp
        LIBS Aurora.Adaptor.Static Framework 3rdParty::googletest
        WORKING_DIR $<TARGET_FILE_DIR:Aurora.Renderer.ModuleTest>)
    if (SKY_MODULE_PACKAGING STREQUAL "static")
        target_compile_definitions(Aurora.Renderer.ModuleTest PRIVATE AURORA_RENDERER_PACKAGED_STATIC)
        target_link_libraries(Aurora.Renderer.ModuleTest Aurora.Render Aurora.Metadata)
    else()
        add_dependencies(Aurora.Renderer.ModuleTest Aurora.Render Aurora.Metadata)
    endif()

    # Verify tagged static registration even when production modules are dynamic.
    sky_add_test(TARGET Aurora.Renderer.StaticModuleTest
        SOURCES ../core/test/renderer/RendererModuleTest.cpp src/AuroraRegistry.cpp
        LIBS Aurora.Adaptor.Static Framework 3rdParty::googletest
        WORKING_DIR $<TARGET_FILE_DIR:Aurora.Renderer.StaticModuleTest>)
    target_compile_definitions(Aurora.Renderer.StaticModuleTest PRIVATE
        AURORA_RENDERER_STATIC_FIXTURE SKY_MODULE_STATIC SKY_MODULE_TAG=Aurora_Render_Contract)
    sky_source_compile_variant(TARGET Aurora.Renderer.StaticModuleTest OWNER Aurora.Render
        SOURCE src/AuroraRegistry.cpp
        DEFINITIONS SKY_MODULE_TAG=Aurora_Render_Contract
        REASON "Exercise the tagged static factory independently of the production module packaging")
endif()
