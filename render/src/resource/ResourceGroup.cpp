//
// Created by Zach Lee on 2023/8/28.
//

#include <render/resource/ResourceGroup.h>
#include <render/Renderer.h>

namespace sky {

    ResourceGroup::~ResourceGroup()
    {
        Renderer::Get()->GetResourceGC()->CollectDescriptorSet(set);
    }

    void ResourceGroup::Bind(rhi::GraphicsEncoder& encoder, uint32_t index)
    {
        encoder.BindSet(index, set);
    }

    void ResourceGroup::Bind(rhi::ComputeEncoder& encoder, uint32_t index)
    {
    }
} // namespace sky
