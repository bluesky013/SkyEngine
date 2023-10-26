//
// Created by Zach Lee on 2023/8/27.
//

#pragma once

#include <render/RenderScene.h>

namespace sky {
    class GameObject;

    RenderScene *GetRenderSceneFromGameObject(GameObject *go);

    template <typename T>
    T *GetFeatureProcessor(const RenderScene *scene)
    {
        return scene->GetFeature<T>();
    }

} // namespace sky