//
// Created by Zach Lee on 2022/7/27.
//

#include <RDSceneSample.h>
#include <render/Render.h>
#include <render/features/StaticMeshFeature.h>
#include <framework/window/NativeWindow.h>

namespace sky {

    void RDSceneSample::Init()
    {
        StartInfo info = {};
        info.appName = "RDSceneSample";

        Render::Get()->Init(info);
    }

    void RDSceneSample::Start()
    {
        mainCamera = std::make_shared<RenderView>();
        scene = std::make_shared<RenderScene>();
        scene->AddView(mainCamera);

        viewport = std::make_unique<RenderViewport>();
        auto nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
        RenderViewport::ViewportInfo info = {};
        info.wHandle = nativeWindow->GetNativeHandle();
        viewport->Setup(info);
        viewport->SetScene(scene);

        Render::Get()->AddScene(scene);
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