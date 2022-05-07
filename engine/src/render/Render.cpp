
//
// Created by Zach Lee on 2021/11/12.
//

#include <engine/render/Render.h>
#include <engine/world/Viewport.h>
#include <core/logger/Logger.h>
#include <engine/render/DriverManager.h>
#include <engine/render/rendergraph/ForwardRendering.h>
#include <engine/render/DevObjManager.h>
#include <engine/render/service/TransformService.h>
#include <engine/render/service/ViewService.h>

static const char* TAG = "Render";

namespace sky {

    Render::~Render()
    {
        scenes.clear();
        DevObjManager::Get()->Destroy();
        DriverManager::Get()->Destroy();
    }

    bool Render::Init(const StartInfo& info)
    {
        LOG_I(TAG, "Init Render");
        if (!DriverManager::Get()->Initialize({info.appName})) {
            LOG_E(TAG, "Init Driver Failed");
            return false;
        }
        return true;
    }

    void Render::OnAddWorld(World& world)
    {
        auto iter = scenes.find(&world);
        if (iter != scenes.end()) {
            return;
        }
        auto scene = std::make_unique<RenderScene>();
        auto pipeline = std::make_unique<ForwardRendering>();
        scene->SetRenderPipeline(std::move(pipeline));

        world.GetServiceManager()->RegisterService<TransformService>();
        world.GetServiceManager()->RegisterService<ViewService>();

        scenes.emplace(&world, std::move(scene));
    }

    void Render::OnRemoveWorld(World& world)
    {
        auto iter = scenes.find(&world);
        if (iter == scenes.end()) {
            return;
        }

        world.GetServiceManager()->UnRegisterService<TransformService>();
        world.GetServiceManager()->UnRegisterService<ViewService>();
        scenes.erase(iter);
    }

    void Render::OnAddViewport(Viewport& vp)
    {
        auto handle = vp.GetNativeWindow();
        if (handle == nullptr) {
            return;
        }
        auto iter = swapChains.find(handle);
        if (iter != swapChains.end()) {
            return;
        }
        drv::SwapChain::Descriptor swcDes = {};
        swcDes.window = handle;
        auto swapChain = DriverManager::Get()->CreateDeviceObject<drv::SwapChain>(swcDes);
        swapChains.emplace(handle, swapChain);
    }

    void Render::OnRemoveViewport(Viewport& vp)
    {
        auto handle = vp.GetNativeWindow();
        if (handle == nullptr) {
            return;
        }
        auto iter = swapChains.find(handle);
        if (iter == swapChains.end()) {
            return;
        }
        swapChains.erase(iter);
    }

    void Render::OnWorldTargetChange(World& world, Viewport& vp)
    {
        auto sIt = scenes.find(&world);
        if (sIt == scenes.end()) {
            return;
        }

        auto vIt = swapChains.find(vp.GetNativeWindow());
        if (vIt == swapChains.end()) {
            return;
        }

        sIt->second->SetTarget(vIt->second);
    }

    void Render::OnTick(float time)
    {
        DevObjManager::Get()->TickFreeList();

        for (auto& rs : scenes) {
            rs.second->OnPreTick();
        }

        for (auto& rs : scenes) {
            rs.second->OnTick(time);
        }

        for (auto& rs : scenes) {
            rs.second->OnPostTick();
        }
    }

    void Render::OnWindowResize(void* hwnd, uint32_t w, uint32_t h)
    {
        auto iter = swapChains.find(hwnd);
        if (iter == swapChains.end()) {
            return;
        }
        iter->second->Resize(w, h);
    }
}