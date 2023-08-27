//
// Created by Zach Lee on 2023/8/17.
//

#include <render/Renderer.h>
#include <render/RHI.h>

namespace sky {

    Renderer::Renderer() : scenes(&mainPool), windows(&mainPool)
    {
    }

    Renderer::~Renderer()
    {
        device->WaitIdle();
    }

    void Renderer::Init()
    {
        device = RHI::Get()->GetDevice();
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
        for (auto &scn : scenes) {
            scn->Render();
        }
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
    RenderWindow *Renderer::CreateRenderWindow(void *hWnd, uint32_t width, uint32_t height, bool vSync)
    {
        windows.emplace_back(new RenderWindow(), &Renderer::DestroyObj<RenderWindow>);
        auto *rw = windows.back().get();
        rw->Init(hWnd, width, height, vSync);
        return rw;
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