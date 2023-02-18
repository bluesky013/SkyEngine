//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Core.h>
#include <rhi/BufferView.h>
#include <rhi/Buffer.h>
#include <rhi/ImageView.h>
#include <rhi/Sampler.h>
#include <rhi/DescriptorSetLayout.h>

namespace sky::rhi {

    class DescriptorSet {
    public:
        DescriptorSet() = default;
        virtual ~DescriptorSet() = default;

        struct Descriptor {
            DescriptorSetLayoutPtr layout;
        };

        virtual void BindBuffer(uint32_t binding, DescriptorType type, const BufferViewPtr &view, uint32_t index = 0) = 0;
        virtual void BindImageView(uint32_t binding, DescriptorType type, const ImageViewPtr &view, uint32_t index = 0) = 0;
        virtual void BindSampler(uint32_t binding, DescriptorType type, const SamplerPtr &sampler, uint32_t index = 0) = 0;
    };

}
