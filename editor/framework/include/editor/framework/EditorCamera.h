//
// Created by blues on 2024/7/10.
//

#pragma once

#include <framework/controller/SimpleController.h>
#include <render/SceneView.h>
#include <render/RenderScene.h>

namespace sky::editor {

    class EditorCamera {
    public:
        EditorCamera() = default;
        ~EditorCamera() = default;

        void Init(RenderScene* scene, NativeWindow *window);
        void Shutdown();
        void Tick(float time);
        void UpdateAspect(uint32_t width, uint32_t height);

    private:
        FirstPersonController controller;
        SceneView *sceneView = nullptr;
        RenderScene *renderScene = nullptr;
        Transform transform = {};

        float near = 0.1f;
        float far = 10000.f;
        float fov = 60.f;
        float aspect = 1.f;
    };

} // namespace sky::editor