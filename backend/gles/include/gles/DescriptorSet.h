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

    struct SetDescriptor {
        TextureDescriptor texture;
        BufferDescriptor buffer;
    };

    class DescriptorSet : public rhi::DescriptorSet, public DevObject {
    public:
        DescriptorSet(Device &dev) : DevObject(dev) {}
        ~DescriptorSet() = default;

        void BindBuffer(uint32_t binding, const rhi::BufferViewPtr &view, uint32_t index) override;
        void BindImageView(uint32_t binding, const rhi::ImageViewPtr &view, uint32_t index) override;
        void BindSampler(uint32_t binding, const rhi::SamplerPtr &sampler, uint32_t index) override;

    private:
        bool Init(const Descriptor &desc);
        SetDescriptor &Get(uint32_t binding, uint32_t index);

        DescriptorSetLayoutPtr layout;

        std::vector<SetDescriptor> descriptors;
    };
    using DescriptorSetPtr = std::shared_ptr<DescriptorSet>;
}
