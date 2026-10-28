//
// Aurora Metal ResourceGroup.
//

#pragma once

#include <MetalShader.h>
#include <aurora/rhi/ResourceGroup.h>
#include <core/template/ReferenceObject.h>

#include <array>
#include <unordered_map>
#include <vector>

namespace sky::aurora {

    class MetalDevice;
    class MetalBuffer;
    class MetalImage;
    class MetalSampler;
    class MetalDescriptorEncoder;

    // Logical bindings are remapped to reflected MSL argument slots per stage.
    class MetalResourceGroup : public ResourceGroup {
    public:
        explicit MetalResourceGroup(MetalDevice &dev);
        ~MetalResourceGroup() override;

        bool          Init(const Descriptor &desc);
        const Device *GetDevice() const noexcept override;

        std::unique_ptr<DescriptorEncoder> CreateEncoder() override;

        // Dynamic offsets follow ascending logical binding order, shared by both graphics stages.
        void BindGraphics(void *encoder, uint32_t numDynamicOffsets = 0, const uint32_t *dynamicOffsets = nullptr) const;
        void BindCompute(void *encoder, uint32_t numDynamicOffsets = 0, const uint32_t *dynamicOffsets = nullptr) const;

    private:
        friend class MetalDescriptorEncoder;
        bool ValidateBindings(uint32_t numDynamicOffsets, const uint32_t *dynamicOffsets) const;
        struct BufferBinding {
            CounterPtr<MetalBuffer> buffer;
            uint64_t                offset = 0;
            uint64_t                range  = 0;
        };

        MetalDevice                                           &device;
        CounterPtr<MetalShader>                                shader; // keeps reflection/layout owner alive
        uint32_t                                               setIndex = 0;
        std::unordered_map<uint32_t, BufferBinding>            buffers;
        std::unordered_map<uint32_t, CounterPtr<MetalImage>>   textures;
        std::unordered_map<uint32_t, CounterPtr<MetalSampler>> samplers;
        // bindings typed UNIFORM_BUFFER_DYNAMIC / STORAGE_BUFFER_DYNAMIC,
        // sorted ascending; consumed in order at bind time
        std::vector<uint32_t>           dynamicBindings;
        std::array<MetalMSLSetSlots, 3> stageSlots;

        uint64_t ResolveOffset(
            uint32_t binding, uint64_t recordedOffset, uint32_t numDynamicOffsets, const uint32_t *dynamicOffsets, uint32_t &dynamicCursor) const;
    };

} // namespace sky::aurora
