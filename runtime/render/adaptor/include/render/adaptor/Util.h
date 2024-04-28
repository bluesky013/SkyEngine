//
// Created by Zach Lee on 2023/8/27.
//

#pragma once

#include <render/RenderScene.h>
#include <framework/world/Actor.h>
#include <framework/world/World.h>
#include <render/adaptor/RenderSceneProxy.h>

namespace sky {
    inline RenderScene *GetRenderSceneFromActor(Actor *actor)
    {
        return static_cast<RenderSceneProxy*>(actor->GetWorld()->GetSubSystem("RenderScene"))->GetRenderScene();
    }

    template <typename T>
    T *GetFeatureProcessor(const RenderScene *scene)
    {
        return scene->GetFeature<T>();
    }

    template <typename T>
    T *GetFeatureProcessor(Actor *actor)
    {
        auto *proxy = static_cast<RenderSceneProxy*>(actor->GetWorld()->GetSubSystem("RenderScene"));
        return GetFeatureProcessor<T>(proxy->GetRenderScene());
    }

} // namespace sky