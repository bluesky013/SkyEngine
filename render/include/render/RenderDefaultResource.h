//
// Created by Zach Lee on 2023/9/5.
//

#pragma once

#include <rhi/Device.h>
#include <render/resource/ResourceGroup.h>

namespace sky {

    struct RenderDefaultResource {
        void Init();
        void Reset();

        rhi::DescriptorSetPoolPtr defaultPool;
        RDResourceLayoutPtr emptyDesLayout;
        RDResourceGroupPtr emptySet;

        rhi::SamplerPtr defaultSampler;
        rhi::ImageViewPtr texture2D;
        rhi::ImageViewPtr textureCube;
    };

} // namespace sky
