//
// Created by blues on 2025/2/28.
//

#include "SampleScene.h"
#include <framework/asset/AssetDataBase.h>
#include <framework/window/NativeWindow.h>
#include <render/adaptor/assets/MeshAsset.h>

namespace sky {

    void Sample::Init(NativeWindow* window)
    {
        nativeWindow = window;

        InitWorld();
        InitPipeline();

        Event<IWindowEvent>::Connect(window, this);
    }

    void Sample::InitWorld()
    {
        sceneProxy = new RenderSceneProxy();

        world = World::CreateWorld();
        world->Init();
        world->AddSubSystem(Name(RenderSceneProxy::NAME.data()), sceneProxy);

        auto fs = AssetDataBase::Get()->GetWorkSpaceFs();
        auto file = fs->OpenFile(FilePath("World/t1.world"));
        auto archive = file->ReadAsArchive();
        if (!archive || !archive->IsOpen()) {
            return;
        }
        JsonInputArchive json(*archive);
        world->LoadJson(json);
    }


    void Sample::OnWindowHandleChanged(uint32_t width, uint32_t height, void* handle)
    {
        if (renderWindow != nullptr) {
            renderWindow->UpdateWinHandle(handle);
            renderWindow->Resize(width, height);
        }

        if (profiler) {
            profiler->SetDisplaySize(width, height);
        }

        if (sceneProxy != nullptr) {
            auto *view = sceneProxy->GetRenderScene()->GetSceneView(Name("MainCamera"));
            if (view != nullptr) {
                view->SetPerspective(0.01, 1000, 75 / 180.f * 3.14f,
                    static_cast<float>(width) / static_cast<float>(height));
            }
        }

    }

    void Sample::InitPipeline()
    {
        renderWindow = Renderer::Get()->CreateRenderWindow(nativeWindow->GetNativeHandle(),
            nativeWindow->GetWidth(),
            nativeWindow->GetHeight(), false);

        auto *ppl = new RenderPassPipeline();
        Renderer::Get()->SetPipeline(ppl);

        sceneProxy = static_cast<RenderSceneProxy*>(world->GetSubSystem(Name("RenderScene")));
        auto *scenePipeline = new DefaultForwardPipeline(sceneProxy->GetRenderScene());
        scenePipeline->SetOutput(renderWindow);
        ppl->AddScenePass(scenePipeline);

        profiler = std::make_unique<RenderProfiler>(sceneProxy->GetRenderScene());
        profiler->SetDisplaySize(nativeWindow->GetWidth(), nativeWindow->GetHeight());
    }

    void Sample::Shutdown()
    {
        Renderer::Get()->SetPipeline(nullptr);
        profiler = nullptr;
        sceneProxy = nullptr;
        Renderer::Get()->DestroyRenderWindow(renderWindow);
        Event<IWindowEvent>::DisConnect(this);
    }

    void Sample::Tick(float time)
    {
        if (world) {
            world->Tick(time);
        }

        if (profiler) {
            profiler->Tick();
        }
    }

} // namespace sky
