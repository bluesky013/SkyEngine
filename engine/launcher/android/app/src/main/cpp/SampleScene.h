//
// Created by blues on 2025/2/28.
//

#pragma once

#include <render/adaptor/pipeline/DefaultForwardPipeline.h>
#include <render/adaptor/assets/TechniqueAsset.h>
#include <render/adaptor/RenderSceneProxy.h>
#include <render/adaptor/profile/RenderProfiler.h>
#include <render/RenderScenePipeline.h>
#include <render/RenderPassPipeline.h>
#include <render/Renderer.h>
#include <framework/world/World.h>
#include <framework/window/IWindowEvent.h>

namespace sky {
    class NativeWindow;

    class Sample : public IWindowEvent {
    public:
        Sample() = default;
        ~Sample() = default;

        void Init(NativeWindow* nativeWindow);
        void Shutdown();
        void Tick(float time);

    private:
        void InitWorld();
        void InitPipeline();
        void OnWindowHandleChanged(uint32_t width, uint32_t height, void* handle) override;

        NativeWindow *nativeWindow = nullptr;
        RenderWindow *renderWindow = nullptr;
        RenderSceneProxy* sceneProxy = nullptr;
        std::unique_ptr<RenderProfiler> profiler;
        WorldPtr world;
    };

} // namespace sky
