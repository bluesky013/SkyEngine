add_library(Aurora.Renderer.Adaptor STATIC
    renderer/RendererProvider.cpp
    renderer/RendererProvider.h)
target_link_libraries(Aurora.Renderer.Adaptor PUBLIC Aurora.Renderer.Interface)
set_target_properties(Aurora.Renderer.Adaptor PROPERTIES FOLDER "${CURRENT_FOLDER}")
