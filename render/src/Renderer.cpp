//
// Created by Zach Lee on 2023/8/17.
//

#include <render/Renderer.h>
#include <render/RHI.h>
#include <render/rdg/TransientObjectPool.h>
#include <render/rdg/TransientMemoryPool.h>

namespace sky {

    Renderer::Renderer() : scenes(&mainPool), windows(&mainPool)
    {
    }

    Renderer::~Renderer()
    {
        device->WaitIdle();

        rdgContext = nullptr;
    }

    void Renderer::Init()
    {
        device = RHI::Get()->GetDevice();

        rdgContext = std::make_unique<rdg::RenderGraphContext>();
        rdgContext->pool = std::make_unique<rdg::TransientObjectPool>();
        rdgContext->device = device;
        rdgContext->mainCommandBuffer = device->CreateCommandBuffer({});
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

    void Renderer::Render()
    {

    }

    void Renderer::AfterRender(float time)
    {
        for (auto &scn : scenes) {
            scn->PostTick(time);
        }
    }

    RenderScene *Renderer::CreateScene()
    {
        scenes.emplace_back(new RenderScene(), &Renderer::DestroyObj<RenderScene>);
        return scenes.back().get();
    }
    RenderWindow *Renderer::CreateRenderWindow()
    {
        windows.emplace_back(new RenderWindow(), &Renderer::DestroyObj<RenderWindow>);
        return windows.back().get();
    }

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