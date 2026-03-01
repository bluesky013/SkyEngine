//
// Created by Zach Lee on 2023/8/27.
//

#include <render/adaptor/RenderSceneProxy.h>
#include <render/Renderer.h>

namespace sky {

    RenderSceneProxy::RenderSceneProxy(const Uuid& uuid)
    {
        renderScene = Renderer::Get()->CreateScene(uuid);
    }

    RenderSceneProxy::~RenderSceneProxy()
    {
        Renderer::Get()->RemoveScene(renderScene);
    }

} // namespace sky