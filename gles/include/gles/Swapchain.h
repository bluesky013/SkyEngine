//
// Created by Zach Lee on 2023/1/31.
//

#pragma once

#include <rhi/Swapchain.h>
#include <gles/Forward.h>
#include <gles/DevObject.h>
#include <gles/Surface.h>

namespace sky::gles {
    class Device;

    class SwapChain : public rhi::SwapChain, public DevObject, public Surface {
    public:
        SwapChain(Device &dev) : DevObject(dev) {}
        ~SwapChain();

        bool Init(const Descriptor &desc) override;
    };
    using SwapChainPtr = std::shared_ptr<SwapChain>;
}
