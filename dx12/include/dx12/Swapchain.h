//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <dx12/DevObject.h>

namespace sky::dx {
    class Device;

    class SwapChain : public DevObject {
    public:
        SwapChain(Device &device);
        ~SwapChain();

        struct Descriptor {
            void *window = nullptr;
            uint32_t width = 0;
            uint32_t height = 0;
        };

    private:
        friend class Device;
        bool Init(const Descriptor &);

        ComPtr<IDXGISwapChain1> swapChain;
    };
    using SwapChainPtr = std::shared_ptr<SwapChain>;
}