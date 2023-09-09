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
        BufferPtr buffer;
        uint64_t offset;
        uint64_t size;
    };

    struct SetDescriptor {
        TextureDescriptor texture;
        BufferDescriptor buffer;
    };

    class DescriptorSet : public rhi::DescriptorSet, public DevObject {
    public:
        explicit DescriptorSet(Device &dev) : DevObject(dev) {}
        ~DescriptorSet() override = default;

        void BindBuffer(uint32_t binding, const rhi::BufferViewPtr &view, uint32_t index) override;
        void BindBuffer(uint32_t binding, const rhi::BufferPtr &buffer, uint64_t offset, uint64_t size, uint32_t index) override;
        void BindImageView(uint32_t binding, const rhi::ImageViewPtr &view, uint32_t index, rhi::DescriptorBindFlags flags) override;
        void BindSampler(uint32_t binding, const rhi::SamplerPtr &sampler, uint32_t index) override;
        void Update() override;

        const std::vector<SetDescriptor> &GetDescriptors() const { return descriptors; }

    private:
        friend class DescriptorSetPool;
        bool Init(const Descriptor &desc);
        SetDescriptor &Get(uint32_t binding, uint32_t index);

        DescriptorSetLayoutPtr layout;

        std::vector<SetDescriptor> descriptors;
        bool dirty = true;
    };
    using DescriptorSetPtr = std::shared_ptr<DescriptorSet>;
}
