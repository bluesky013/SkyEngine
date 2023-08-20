//
// Created by Zach Lee on 2023/4/2.
//

#pragma once

#include <vector>
#include <core/std/Container.h>
#include <render/SceneView.h>
#include <render/RenderPrimitive.h>

namespace sky {

    class RenderScene {
    public:
        void PreTick(float time);
        void PostTick(float time);

        void AddSceneView(SceneView *view);
        void RemoveSceneView(SceneView *view);

        void AddPrimitive(RenderPrimitive *primitive);
        void RemovePrimitive(RenderPrimitive *primitive);

    private:
        friend class Renderer;
        RenderScene();
        ~RenderScene() = default;

        PmrUnSyncPoolRes resources;

        PmrVector<SceneView *> sceneViews;
        PmrVector<RenderPrimitive *> primitives;
    };

} // namespace sky
