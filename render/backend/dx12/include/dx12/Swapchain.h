//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <rhi/Swapchain.h>
#include <dx12/DevObject.h>

namespace sky::dx {
    class Device;

    class SwapChain : public rhi::SwapChain, public DevObject {
    public:
        SwapChain(Device &device);
        ~SwapChain() override;

    private:
        friend class Device;
        bool Init(const Descriptor &);

        ComPtr<IDXGISwapChain1> swapChain;
    };
    using SwapChainPtr = std::shared_ptr<SwapChain>;
}