//
// Created by Zach on 2023/2/6.
//

#pragma once

#include <rhi/DescriptorSet.h>
#include <gles/DevObject.h>
#include <gles/ImageView.h>
#include <gles/BufferView.h>
#include <gles/Sampler.h>
#include <gles/DescriptorSetLayout.h>
#include <unordered_map>

namespace sky::gles {

    struct TextureDescriptor {
        ImageViewPtr view;
        SamplerPtr sampler;
    };

    struct BufferDescriptor {
        GLenum type;
        BufferViewPtr view;
    };

    union Descriptor {
        TextureDescriptor texDesc;
        BufferDescriptor bufferDesc;
    };

    class DescriptorSet : public rhi::DescriptorSet, public DevObject {
    public:
        DescriptorSet(Device &dev) : DevObject(dev) {}
        ~DescriptorSet() = default;

        void BindBuffer(uint32_t binding, rhi::DescriptorType type, const rhi::BufferViewPtr &view, uint32_t index) override;
        void BindImageView(uint32_t binding, rhi::DescriptorType type, const rhi::ImageViewPtr &view, uint32_t index) override;
        void BindSampler(uint32_t binding, rhi::DescriptorType type, const rhi::SamplerPtr &sampler, uint32_t index) override;

    private:
        bool Init(const Descriptor &desc);
        DescriptorSetLayoutPtr layout;
    };

}
