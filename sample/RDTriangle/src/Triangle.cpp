//
// Created by Zach Lee on 2022/6/16.
//


#include <Triangle.h>

namespace sky::render {

    void Triangle::Init()
    {
        StartInfo info = {};
        info.appName = "RDTriangle";
        Render::Get()->Init(info);
    }

    void Triangle::Start()
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

    void Triangle::Stop()
    {
        viewport->Shutdown();
        viewport = nullptr;
        scene = nullptr;
        mainCamera = nullptr;

        Render::Get()->Destroy();
    }

    void Triangle::Tick(float delta)
    {
        Render::Get()->OnTick(delta);
    }


}