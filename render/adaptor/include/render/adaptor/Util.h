//
// Created by Zach Lee on 2023/8/27.
//

#pragma once

#include <render/RenderScene.h>

namespace sky {
    class Actor;

    RenderScene *GetRenderSceneFromGameObject(Actor *go);

    template <typename T>
    T *GetFeatureProcessor(const RenderScene *scene)
    {
        return scene->GetFeature<T>();
    }

} // namespace sky