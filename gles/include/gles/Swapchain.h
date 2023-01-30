//
// Created by Zach Lee on 2023/1/31.
//

#pragma once

#include <rhi/Swapchain.h>
#include <gles/Forward.h>

namespace sky::gles {

    class SwapChain : public rhi::SwapChain {
    public:
        SwapChain() = default;
        ~SwapChain() = default;

        bool Init(const Descriptor &desc) override;
    };

}
