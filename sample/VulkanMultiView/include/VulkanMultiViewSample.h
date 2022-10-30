//
// Created by Zach Lee on 2022/6/16.
//

#pragma once

#include "VulkanSampleBase.h"

namespace sky {
    class NativeWindow;

    class VulkanMultiViewSample : public VulkanSampleBase {
    public:
        VulkanMultiViewSample()  = default;
        ~VulkanMultiViewSample() = default;

        void Tick(float delta) override;

        void OnStart() override;
        void OnStop() override;

    private:
        drv::FrameBufferPtr multiViewFrameBuffer;
        drv::ImagePtr colorImage;
        drv::ImageViewPtr view;

    };

} // namespace sky
