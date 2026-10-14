sky_add_module(TARGET Aurora.Renderer
    SOURCES RendererModule.cpp
    PRIVATE_INC ../adaptor/renderer
    LINK_LIBS Aurora.Renderer.Adaptor Framework)

if (SKY_BUILD_TEST)
    sky_add_test(TARGET Aurora.Renderer.ModuleTest
        SOURCES ../test/RendererModuleTest.cpp
        LIBS Aurora.Renderer.Interface Framework 3rdParty::googletest
        WORKING_DIR $<TARGET_FILE_DIR:Aurora.Renderer.ModuleTest>)
    if (SKY_MODULE_PACKAGING STREQUAL "static")
        target_compile_definitions(Aurora.Renderer.ModuleTest PRIVATE AURORA_RENDERER_PACKAGED_STATIC)
        target_link_libraries(Aurora.Renderer.ModuleTest Aurora.Renderer)
    else()
        add_dependencies(Aurora.Renderer.ModuleTest Aurora.Renderer)
    endif()

    # Verify tagged static registration even when production modules are dynamic.
    sky_add_test(TARGET Aurora.Renderer.StaticModuleTest
        SOURCES ../test/RendererModuleTest.cpp RendererModule.cpp
        INCS ../adaptor/renderer
        LIBS Aurora.Renderer.Adaptor Framework 3rdParty::googletest
        WORKING_DIR $<TARGET_FILE_DIR:Aurora.Renderer.StaticModuleTest>)
    target_compile_definitions(Aurora.Renderer.StaticModuleTest PRIVATE
        AURORA_RENDERER_STATIC_FIXTURE SKY_MODULE_STATIC SKY_MODULE_TAG=Aurora_Renderer_Contract)
endif()
