//
// Created by Zach Lee on 2023/8/17.
//

#include <render/Renderer.h>
#include <render/RHI.h>
#include <render/rdg/RenderGraph.h>
#include <core/profile/Profiler.h>

namespace sky {

    Renderer::Renderer()
        : scenes(&mainPool)
        , windows(&mainPool)
        , delayReleaseCollections(&mainPool)
        , features(&mainPool)
    {
    }

    Renderer::~Renderer()
    {
        pipeline = nullptr;
        streamManager = nullptr;
        defaultResource.Reset();
        features.clear();
        scenes.clear();
        windows.clear();
        delayReleaseCollections.clear();
    }

    void Renderer::Init()
    {
        device = RHI::Get()->GetDevice();
        defaultResource.Init();
        delayReleaseCollections.resize(inflightFrameCount);
        for (uint32_t i = 0; i < inflightFrameCount; ++i) {
            delayReleaseCollections[i] = std::make_unique<RenderResourceGC>();
        }

        streamManager = std::make_unique<RenderStreamManager>();
        streamManager->SetUploadQueue(device->GetQueue(rhi::QueueType::TRANSFER));

        materialManager = std::make_unique<MaterialManager>();
        materialManager->Init();
    }

    void Renderer::Tick(float time)
    {
        BeforeRender(time);
        Render();
        AfterRender(time);
    }

    void Renderer::BeforeRender(float time)
    {
        for (auto &scn : scenes) {
            scn->PreTick(time);
        }
    }

    void Renderer::SetPipeline(RenderPipeline *ppl)
    {
        device->WaitIdle();
        pipeline.reset(ppl);
    }

    void Renderer::Render()
    {
        if (pipeline == nullptr || scenes.empty()) {
            return;
        }

        {
            pipeline->FrameSync();
        }

        rdg::RenderGraph rdg(pipeline->Context());

        std::vector<RenderScene*> renderScenes;
        for (auto &scn : scenes) {
            scn->Render(rdg);

            renderScenes.emplace_back(scn.get());
        }

        {
            SKY_PROFILE_NAME("pipeline compile")
            pipeline->OnSetup(rdg, renderScenes);
            pipeline->Compile(rdg);
        }

        {
            SKY_PROFILE_NAME("pipeline collect")
            pipeline->Collect(rdg, renderScenes);
        }

        {
            SKY_PROFILE_NAME("pipeline execute")
            pipeline->Execute(rdg);
        }
    }

    void Renderer::AfterRender(float time)
    {
        for (auto &scn : scenes) {
            scn->PostTick(time);
        }
        streamManager->Tick();

        delayReleaseCollections[(frameIndex + inflightFrameCount - 1) % inflightFrameCount]->Clear();

        totalFrame++;
        frameIndex = totalFrame % inflightFrameCount;
    }

    void Renderer::StopRender()
    {
        device->WaitIdle();
    }

    RenderResourceGC *Renderer::GetResourceGC() const
    {
        return delayReleaseCollections[frameIndex].get();
    }

    RenderScene *Renderer::CreateScene()
    {
        scenes.emplace_back(new RenderScene(), &Renderer::DestroyObj<RenderScene>);
        auto &scene = scenes.back();
        for (auto &feature : features) {
            scene->AddFeature(feature->BuildFeatureProcessor(scene.get()));
        }
        return scenes.back().get();
    }
    RenderWindow *Renderer::CreateRenderWindow(void *hWnd, uint32_t width, uint32_t height, bool vSync)
    {
        windows.emplace_back(new RenderWindow(), &Renderer::DestroyObj<RenderWindow>);
        auto *rw = windows.back().get();
        rw->Init(hWnd, width, height, vSync);
        return rw;
    }
#ifdef SKY_ENABLE_XR
    RenderWindow *Renderer::CreateRenderWindowByXR()
    {
        auto *renderWindow = new RenderWindow();
        if (!renderWindow->InitByXR(RHI::Get()->GetDevice()->CreateXRSwapChain({rhi::PixelFormat::BGRA8_UNORM}))) {
            return nullptr;
        }
        windows.emplace_back(renderWindow, &Renderer::DestroyObj<RenderWindow>);
        return renderWindow;
    }
#endif
    void Renderer::RemoveScene(sky::RenderScene *scene)
    {
        scenes.remove_if([scene](const auto &scn) {
            return scene == scn.get();
        });
    }
    void Renderer::DestroyRenderWindow(RenderWindow *window)
    {
        windows.remove_if([window](const auto &wd) {
            return window == wd.get();
        });
    }
} // namespace sky
