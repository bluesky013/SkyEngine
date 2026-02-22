//
// Created by Zach Lee on 2025/7/20.
//

#include <mtl/DescriptorSet.h>

namespace sky::mtl {

    DescriptorSet::~DescriptorSet()
    {
    }

    bool DescriptorSet::Init(const Descriptor& desc)
    {
        setLayout = std::static_pointer_cast<DescriptorSetLayout>(desc.layout);
        return true;
    }

    void DescriptorSet::BindBuffer(uint32_t binding, const rhi::BufferView &view, uint32_t index)
    {

    }

    void DescriptorSet::BindBuffer(uint32_t binding, const rhi::BufferPtr &buffer, uint64_t offset, uint64_t size, uint32_t index)
    {

    }

    void DescriptorSet::BindImageView(uint32_t binding, const rhi::ImageViewPtr &view, uint32_t index, rhi::DescriptorBindFlags flags)
    {

    }

    void DescriptorSet::BindSampler(uint32_t binding, const rhi::SamplerPtr &sampler, uint32_t index)
    {

    }

    void DescriptorSet::Update()
    {

    }

} // namespace sky::mtl
