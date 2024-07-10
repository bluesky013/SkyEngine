//
// Created by blues on 2024/7/10.
//

#include <editor/framework/EditorCamera.h>

namespace sky::editor {

    void EditorCamera::Init(RenderScene* scene, NativeWindow *window)
    {
        controller.BindWindow(window);
        renderScene = scene;
        if (sceneView != nullptr) {
            renderScene->RemoveSceneView(sceneView);
        }
        sceneView = renderScene->CreateSceneView(1);
    }

    void EditorCamera::Shutdown()
    {
        if (sceneView != nullptr) {
            renderScene->RemoveSceneView(sceneView);
            sceneView = nullptr;
        }
    }

} // namespace sky::editor