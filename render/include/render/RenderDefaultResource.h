//
// Created by Zach Lee on 2023/9/5.
//

#pragma once

#include <rhi/Device.h>

namespace sky {

    struct RenderDefaultResource {
        void Init();
        void Reset();

        rhi::DescriptorSetPoolPtr defaultPool;
        rhi::DescriptorSetPtr emptySet;
        rhi::DescriptorSetLayoutPtr emptyDesLayout;
        rhi::SamplerPtr defaultSampler;
        rhi::ImageViewPtr texture2D;
        rhi::ImageViewPtr textureCube;
    };

} // namespace sky
