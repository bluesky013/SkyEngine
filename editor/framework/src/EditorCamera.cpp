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
        Active();

        uint32_t width = window->GetWidth();
        uint32_t height = window->GetHeight();

        transform.translation.x = 0.f;
        transform.translation.y = 150.f;
        transform.translation.z = 100.f;

        sceneView->SetMatrix(transform.ToMatrix());
        sceneView->SetPerspective(near, far, fov / 180.f * 3.14f,
            static_cast<float>(width) / static_cast<float>(height));
    }

    void EditorCamera::Shutdown()
    {
        if (sceneView != nullptr) {
            renderScene->DetachSceneView(sceneView, viewName);
            renderScene->RemoveSceneView(sceneView);
            sceneView = nullptr;
        }
    }

    void EditorCamera::Active()
    {
        if (sceneView != nullptr) {
            renderScene->AttachSceneView(sceneView, viewName);
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
        screenWidth = width;
        screenHeight = height;
        if (sceneView != nullptr) {
            sceneView->SetPerspective(near, far, fov / 180.f * 3.14f,
                static_cast<float>(width) / static_cast<float>(height));
        }
    }

    const Matrix4 &EditorCamera::GetProjectMatrix() const
    {
        return sceneView->GetProject();
    }

    const Matrix4 &EditorCamera::GetWorldMatrix() const
    {
        return sceneView->GetWorld();
    }

    const Matrix4 &EditorCamera::GetViewProjectMatrix() const
    {
        return sceneView->GetViewProject();
    }

} // namespace sky::editor