//
// Created by Zach Lee on 2023/8/17.
//

#pragma once

#include <rhi/Device.h>

namespace sky {
    class RenderWindow {
    public:
        bool Init(void *hWnd, uint32_t width, uint32_t height, bool vSync);
        void Resize(uint32_t width, uint32_t height);

        uint32_t GetWidth() const;
        uint32_t GetHeight() const;
        rhi::PixelFormat GetOutputFormat() const;
        const rhi::SwapChainPtr &GetSwaChain() const { return swapChain; }

    private:
        friend class Renderer;
        RenderWindow() = default;
        ~RenderWindow() = default;

        rhi::SwapChainPtr swapChain;
        void *winHandle = nullptr;
    };

} // namespace sky