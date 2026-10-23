#pragma once

#include <MetalBuffer.h>
#include <MetalImage.h>
#include <MetalResourceGroup.h>
#include <MetalSampler.h>
#include <aurora/rhi/DescriptorEncoder.h>

namespace sky::aurora {
    class MetalDescriptorEncoder : public DescriptorEncoder {
    public:
        explicit MetalDescriptorEncoder(MetalResourceGroup &group);
        ~MetalDescriptorEncoder() override = default;
        bool               WriteBuffer(uint32_t binding, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement = 0) override;
        bool               WriteImage(uint32_t binding, Image *image, ImageLayout layout, uint32_t arrayElement = 0) override;
        bool               WriteSampler(uint32_t binding, Sampler *sampler, uint32_t arrayElement = 0) override;
        bool               End() override;
        const std::string &GetError() const override
        {
            return error;
        }
        void Cancel() override
        {
            ended = true;
        }

    private:
        friend class MetalDescriptorBatch;
        const ShaderResource                                           *Find(uint32_t binding, uint32_t arrayElement);
        bool                                                            Fail(const char *message);
        bool                                                            Ready();
        void                                                            Commit();
        CounterPtr<MetalResourceGroup>                                  group;
        std::unordered_map<uint32_t, MetalResourceGroup::BufferBinding> buffers;
        std::unordered_map<uint32_t, CounterPtr<MetalImage>>            textures;
        std::unordered_map<uint32_t, CounterPtr<MetalSampler>>          samplers;
        std::string                                                     error;
        bool                                                            ended = false;
    };
} // namespace sky::aurora
