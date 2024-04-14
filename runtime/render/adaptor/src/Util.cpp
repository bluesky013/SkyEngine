//
// Created by Zach Lee on 2023/8/27.
//

#include <render/adaptor/Util.h>
#include <render/adaptor/RenderSceneProxy.h>
#include <framework/world/World.h>
#include <framework/world/Actor.h>

namespace sky {

    RenderScene *GetRenderSceneFromGameObject(Actor *go)
    {
        return static_cast<RenderSceneProxy*>(go->GetWorld()->GetRenderScene())->GetRenderScene();
    }

} // namespace sky
