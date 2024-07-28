//
// Created by blues on 2024/7/10.
//

#include <editor/framework/EditorCamera.h>
#include <framework/window/NativeWindow.h>

namespace sky::editor {

    void EditorCamera::Init(RenderScene* scene, NativeWindow *window)
    {
        controller.BindWindow(window);
        renderScene = scene;
        if (sceneView != nullptr) {
            renderScene->RemoveSceneView(sceneView);
        }
        sceneView = renderScene->CreateSceneView(1);

        uint32_t width = window->GetWidth();
        uint32_t height = window->GetHeight();
        sceneView->SetMatrix(Matrix4::Identity());
        sceneView->SetPerspective(near, far, fov / 180.f * 3.14f,
            static_cast<float>(width) / static_cast<float>(height));
    }

    void EditorCamera::Shutdown()
    {
        if (sceneView != nullptr) {
            renderScene->RemoveSceneView(sceneView);
            sceneView = nullptr;
        }
    }

    void EditorCamera::Tick(float time)
    {
        if (sceneView != nullptr) {
            transform = controller.Resolve(time, transform);
            sceneView->SetMatrix(transform.ToMatrix());
        }
    }

    void EditorCamera::UpdateAspect(uint32_t width, uint32_t height)
    {
        if (sceneView != nullptr) {
            sceneView->SetPerspective(near, far, fov / 180.f * 3.14f,
                static_cast<float>(width) / static_cast<float>(height));
        }
    }

} // namespace sky::editor