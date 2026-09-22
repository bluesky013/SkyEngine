//
// Aurora Metal ResourceGroup.
//

#pragma once

#include <aurora/rhi/ResourceGroup.h>
#include <core/template/ReferenceObject.h>

#include <unordered_map>
#include <vector>

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

        // Descriptor arrays: slang MSL lowers a resource array to consecutive
        // argument indices starting at the array's base, so element e of
        // `binding` occupies slot binding + e in the direct-binding table.
        void WriteBuffer(uint32_t binding, MetalBuffer *buffer, uint64_t offset, uint32_t arrayElement = 0);
        void WriteImage(uint32_t binding, MetalImage *image, uint32_t arrayElement = 0);
        void WriteSampler(uint32_t binding, MetalSampler *sampler, uint32_t arrayElement = 0);

        // apply recorded bindings; encoder is id<MTLRenderCommandEncoder> /
        // id<MTLComputeCommandEncoder> kept as void* to hide Obj-C types.
        // dynamicOffsets are consumed in reflection order (sorted by binding)
        // for bindings typed *_DYNAMIC, matching the DX12 root CBV/UAV path.
        void BindGraphics(void *encoder, uint32_t numDynamicOffsets = 0, const uint32_t *dynamicOffsets = nullptr) const;
        void BindCompute(void *encoder, uint32_t numDynamicOffsets = 0, const uint32_t *dynamicOffsets = nullptr) const;

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
        // bindings typed UNIFORM_BUFFER_DYNAMIC / STORAGE_BUFFER_DYNAMIC,
        // sorted ascending; consumed in order at bind time
        std::vector<uint32_t>                                  dynamicBindings;

        uint64_t ResolveOffset(uint32_t binding, uint64_t recordedOffset,
                               uint32_t numDynamicOffsets, const uint32_t *dynamicOffsets,
                               uint32_t &dynamicCursor) const;
    };

} // namespace sky::aurora
