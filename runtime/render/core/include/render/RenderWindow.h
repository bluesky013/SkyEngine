//
// Created by Zach Lee on 2023/8/17.
//

#pragma once

#include <rhi/Device.h>

namespace sky {
    class RenderWindow {
    public:
        bool Init(void *hWnd, uint32_t width, uint32_t height, bool vSync);
#ifdef SKY_ENABLE_XR
        bool InitByXR(const rhi::XRSwapChainPtr &xr);
#endif

        void Resize(uint32_t width, uint32_t height);

        uint32_t GetWidth() const;
        uint32_t GetHeight() const;
        const rhi::SwapChainPtr &GetSwapChain() const { return swapChain; }

#ifdef SKY_ENABLE_XR
        const rhi::XRSwapChainPtr &GetXRSwaChain() const { return xrSwapChain; }
#endif

    private:
        friend class Renderer;
        RenderWindow() = default;
        ~RenderWindow() = default;

        rhi::SwapChainPtr swapChain;
        void *winHandle = nullptr;

#ifdef SKY_ENABLE_XR
        rhi::XRSwapChainPtr xrSwapChain;
#endif
    };

} // namespace sky