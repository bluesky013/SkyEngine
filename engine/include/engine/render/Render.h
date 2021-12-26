//
// Created by Zach Lee on 2021/11/12.
//


#pragma once
#include <engine/SkyEngine.h>
#include <framework/environment/Singleton.h>
#include <engine/render/RenderScene.h>
#include <vulkan/Swapchain.h>
#include <unordered_map>

namespace sky {

    namespace drv {
        class Driver;
        class Device;
    }

    class RenderScene;

    class Render : public Singleton<Render>, public IEngineEvent {
    public:
        bool Init(const StartInfo&);

        void OnAddWorld(World&) override;
        void OnRemoveWorld(World&) override;

        void OnAddViewport(Viewport&) override;
        void OnRemoveViewport(Viewport&) override;

        void OnWorldTargetChange(World& world, Viewport& vp) override;

        void OnWindowResize(void* hwnd, uint32_t, uint32_t) override;

        void OnTick(float time) override;

        static void Reflect();

    private:
        friend class Singleton<Render>;
        Render() = default;
        ~Render();
        using RenderScenePtr = std::unique_ptr<RenderScene>;

        std::unordered_map<void*, drv::SwapChainPtr> swapChains;
        std::unordered_map<World*, RenderScenePtr> scenes;
    };

}