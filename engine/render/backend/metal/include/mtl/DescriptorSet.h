//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <mtl/DevObject.h>
#include <mtl/DescriptorSetLayout.h>
#include <rhi/DescriptorSet.h>

namespace sky::mtl {

    class DescriptorSet : public rhi::DescriptorSet, public DevObject {
    public:
        explicit DescriptorSet(Device &device) : DevObject(device) {}
        ~DescriptorSet() override;
        
        bool Init(const Descriptor& desc);

        void BindBuffer(uint32_t binding, const rhi::BufferView &view, uint32_t index) override;
        void BindBuffer(uint32_t binding, const rhi::BufferPtr &buffer, uint64_t offset, uint64_t size, uint32_t index) override;
        void BindImageView(uint32_t binding, const rhi::ImageViewPtr &view, uint32_t index, rhi::DescriptorBindFlags flags) override;
        void BindSampler(uint32_t binding, const rhi::SamplerPtr &sampler, uint32_t index) override;
        void Update() override;

    private:
        DescriptorSetLayoutPtr setLayout;
    };

} // namespace sky::mtl
