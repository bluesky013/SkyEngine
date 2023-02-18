//
// Created by Zach on 2023/2/6.
//

#include <gles/DescriptorSet.h>

namespace sky::gles {
    bool DescriptorSet::Init(const Descriptor &desc)
    {
        layout = std::static_pointer_cast<DescriptorSetLayout>(desc.layout);
        return true;
    }

    void DescriptorSet::BindBuffer(uint32_t binding, rhi::DescriptorType type, const rhi::BufferViewPtr &view, uint32_t index)
    {

    }

    void DescriptorSet::BindImageView(uint32_t binding, rhi::DescriptorType type, const rhi::ImageViewPtr &view, uint32_t index)
    {

    }

    void DescriptorSet::BindSampler(uint32_t binding, rhi::DescriptorType type, const rhi::SamplerPtr &sampler, uint32_t index)
    {

    }
}
