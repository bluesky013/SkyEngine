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

    private:
        FirstPersonController controller;
        SceneView *sceneView = nullptr;
        RenderScene *renderScene = nullptr;
    };

} // namespace sky::editor