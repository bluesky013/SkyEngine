//
// Created by Zach Lee on 2025/9/29.
//

#pragma once

#include <render/RHI.h>
#include <framework/window/IWindowEvent.h>
#include <core/event/Event.h>
#include "GuiRender.h"
#include "ImageRender.h"

namespace sky {

    class NativeWindow;

    class ImageTool : public IWindowEvent, public IDropEvent {
    public:
        ImageTool() = default;
        ~ImageTool() override = default;

        void Init(NativeWindow* window);
        void Shutdown();
        void Tick(float delta);
        void Render();

    private:
        void OnWindowResize(const WindowResizeEvent& event) override;

        void OnDrop(const std::string& payload) override;

        void InitPipeline();

        NativeWindow* mainViewport = nullptr;
        rhi::Device* device = nullptr;
        rhi::SwapChainPtr swapChain;

        rhi::RenderPassPtr renderPass;
        rhi::CommandBufferPtr commandBuffer;
        rhi::FencePtr fence;
        std::vector<rhi::SemaphorePtr> renderSema;
        rhi::SemaphorePtr presentSema;

        rhi::ShaderPtr shader;
        std::vector<rhi::FrameBufferPtr> frameBuffers;

        std::unique_ptr<GuiRender> gui;
        std::unique_ptr<ImageRender> imageRender;

        EventBinder<IWindowEvent> wndEvent;
        EventBinder<IDropEvent> dropEvent;

        uint32_t currentFrame = 0;
    };

} // namespace sky