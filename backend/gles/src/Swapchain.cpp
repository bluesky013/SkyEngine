//
// Created by Zach Lee on 2023/1/31.
//

#include <gles/Swapchain.h>
#include <gles/Device.h>
#include <gles/Core.h>

namespace sky::gles {

    SwapChain::~SwapChain()
    {
    }

    bool SwapChain::Init(const Descriptor &desc)
    {
        if (!CreateSurface(desc.window)) {
            return false;
        }
        auto &ext = surface->GetExtent();

        color = std::make_shared<Image>(device);
        color->imageDesc.extent = {ext.width, ext.height, 1};
        color->imageDesc.usage = rhi::ImageUsageFlagBit::RENDER_TARGET;
        color->imageDesc.format = rhi::PixelFormat::RGBA8_UNORM;
        color->surface = surface;

        depth = std::make_shared<Image>(device);
        depth->imageDesc.extent = {ext.width, ext.height, 1};
        depth->imageDesc.usage = rhi::ImageUsageFlagBit::DEPTH_STENCIL;
        depth->imageDesc.format = rhi::PixelFormat::D24_S8;
        depth->surface = surface;

        return true;
    }

    void SwapChain::Present(rhi::Queue &queue, const rhi::PresentInfo &info)
    {
        auto &glesQueue = static_cast<Queue&>(queue);
        glesQueue.Present(surface);
    }

    bool SwapChain::CreateSurface(void *window)
    {
        auto config = device.GetMainContext()->GetConfig();
        surface = std::make_shared<WindowSurface>();
        if (!surface->Init(config, window)) {
            return false;
        }
        return true;
    }

    void SwapChain::Resize(uint32_t width, uint32_t height, void* window)
    {
        auto *graphicsQueue = device.GetGraphicsQueue();
        auto handle = graphicsQueue->CreateTask([this]() {
            surface = nullptr;
        });

        graphicsQueue->Wait(handle);
        if (!CreateSurface(window)) {
            return;
        }
        color->surface = surface;
        depth->surface = surface;
    }
}
