//
// Created by Zach Lee on 2023/4/2.
//

#pragma once

#include <vector>
#include <core/std/Container.h>
#include <render/SceneView.h>

namespace sky {

    class RenderScene {
    public:
        void PreTick(float time);
        void PostTick(float time);

        void AddSceneView(const ViewPtr &view);
        void RemoveSceneView(const ViewPtr &view);

    private:
        friend class Renderer;
        RenderScene();
        ~RenderScene() = default;

        PmrUnSyncPoolRes resources;
        PmrVector<ViewPtr> sceneViews;
    };

} // namespace sky