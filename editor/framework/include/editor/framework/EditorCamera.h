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
        void Active();

        const Matrix4 &GetProjectMatrix() const;
        const Matrix4 &GetWorldMatrix() const;
        const Matrix4 &GetViewProjectMatrix() const;

        void SetMoveSpeed(float speed) { controller.SetMoveSpeed(speed); }

        uint32_t GetScreenWidth() const { return screenWidth; }
        uint32_t GetScreenHeight() const { return screenHeight; }
    private:
        FirstPersonController controller;
        SceneView *sceneView = nullptr;
        RenderScene *renderScene = nullptr;
        Transform transform = {};

        uint32_t screenWidth = 1;
        uint32_t screenHeight = 1;

        float near = 0.1f;
        float far = 1000.f;
        float fov = 60.f;
        float aspect = 1.f;

        Name viewName{"MainCamera"};
    };

} // namespace sky::editor
