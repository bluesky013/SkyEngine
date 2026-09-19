//
// Aurora Metal ResourceGroup.
//

#pragma once

#include <aurora/rhi/ResourceGroup.h>
#include <core/template/ReferenceObject.h>

#include <unordered_map>

namespace sky::aurora {

    class MetalDevice;
    class MetalShader;
    class MetalBuffer;
    class MetalImage;
    class MetalSampler;

    // slang MSL output flattens resources into per-category sequential indices
    // ([[buffer(N)]] / [[texture(N)]] / [[sampler(N)]], register spaces are
    // ignored), so a Metal ResourceGroup is a set of direct binding tables
    // applied with setBuffer/setTexture/setSampler at BindResourceGroup time.
    // No argument buffer (tier-1 binding model).
    class MetalResourceGroup : public ResourceGroup {
    public:
        explicit MetalResourceGroup(MetalDevice &dev);
        ~MetalResourceGroup() override = default;

        bool Init(const Descriptor &desc);

        std::unique_ptr<DescriptorEncoder> CreateEncoder() override;

        void WriteBuffer(uint32_t binding, MetalBuffer *buffer, uint64_t offset);
        void WriteImage(uint32_t binding, MetalImage *image);
        void WriteSampler(uint32_t binding, MetalSampler *sampler);

        // apply recorded bindings; encoder is id<MTLRenderCommandEncoder> /
        // id<MTLComputeCommandEncoder> kept as void* to hide Obj-C types
        void BindGraphics(void *encoder) const;
        void BindCompute(void *encoder) const;

    private:
        struct BufferBinding {
            CounterPtr<MetalBuffer> buffer;
            uint64_t                offset = 0;
        };

        MetalDevice                                           &device;
        CounterPtr<MetalShader>                                shader; // keeps reflection/layout owner alive
        uint32_t                                               setIndex = 0;
        std::unordered_map<uint32_t, BufferBinding>            buffers;
        std::unordered_map<uint32_t, CounterPtr<MetalImage>>   textures;
        std::unordered_map<uint32_t, CounterPtr<MetalSampler>> samplers;
    };

} // namespace sky::aurora
