//
// Created by Zach Lee on 2023/9/5.
//

#pragma once

#include <rhi/Device.h>
#include <render/resource/ResourceGroup.h>
#include <render/resource/Texture.h>

namespace sky {

    struct RenderDefaultResource {
        void Init();
        void Reset();

        rhi::VertexInputPtr emptyVI;
        rhi::DescriptorSetPoolPtr defaultPool;
        RDResourceLayoutPtr emptyDesLayout;
        RDResourceGroupPtr emptySet;

        rhi::SamplerPtr defaultSampler;
        RDTexture2DPtr texture2D;
    };

} // namespace sky
