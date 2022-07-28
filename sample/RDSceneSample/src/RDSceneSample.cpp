//
// Created by Zach Lee on 2022/7/27.
//

#include <RDSceneSample.h>
#include <render/Render.h>
#include <render/features/StaticMeshFeature.h>
#include <render/features/CameraFeature.h>
#include <framework/window/NativeWindow.h>
#include <render/RenderCamera.h>

namespace sky {

    void RDSceneSample::Init()
    {
        StartInfo info = {};
        info.appName = "RDSceneSample";

        Render::Get()->Init(info);
    }

    void RDSceneSample::Start()
    {
        scene = std::make_shared<RenderScene>();
        Render::Get()->AddScene(scene);

        viewport = std::make_unique<RenderViewport>();
        auto nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();

        RenderViewport::ViewportInfo info = {};
        info.wHandle = nativeWindow->GetNativeHandle();
        viewport->Setup(info);
        viewport->SetScene(scene);
        auto& ext = viewport->GetSwapChain()->GetExtent();

        cmFeature = scene->GetFeature<CameraFeature>();
        smFeature = scene->GetFeature<StaticMeshFeature>();

        mainCamera = cmFeature->Create();
        mainCamera->SetProjectMatrix(glm::perspective(
            60 / 180.f * 3.14f,
            ext.width / static_cast<float>(ext.height),
            0.01f,
            100.f)
        );

        mainCamera->SetTransform(glm::translate(glm::identity<Matrix4>(), Vector3(0, 0, 5)));

        mesh = smFeature->Create();
        Matrix4 transform = glm::identity<Matrix4>();
        transform = glm::translate(transform, Vector3(0.0f, -0.5f, 0.5f));
        transform = glm::rotate(transform, glm::radians(30.f), Vector3(1.f, 1.f, 1.f));

        mesh->SetWorldMatrix(transform);
    }

    void RDSceneSample::Stop()
    {
        scene = nullptr;
        Render::Get()->Destroy();
    }

    void RDSceneSample::Tick(float delta)
    {
        Render::Get()->OnTick(delta);
    }

}

REGISTER_MODULE(sky::RDSceneSample)