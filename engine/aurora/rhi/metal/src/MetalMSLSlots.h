//
// MSL resource argument-slot layout derived from a shader reflection.
//
// slang's Metal target renumbers each resource category independently from 0 in
// declaration order ([[buffer(N)]] / [[texture(N)]] / [[sampler(N)]]), ignoring
// the authored [[vk::binding]] / register space. Both the shader (push constant
// slot + buffer budget) and the resource group (per-category bind slots) must
// derive the same layout, so it is computed here in one place.
//

#pragma once

#include <aurora/rhi/ShaderReflection.h>

#include <cstdint>
#include <unordered_map>

namespace sky::aurora {

    // vk binding -> category index, for a single register space / set
    struct MetalMSLSetSlots {
        std::unordered_map<uint32_t, uint32_t> buffer;
        std::unordered_map<uint32_t, uint32_t> texture;
        std::unordered_map<uint32_t, uint32_t> sampler;

        uint32_t Slot(const std::unordered_map<uint32_t, uint32_t> &map, uint32_t binding) const
        {
            const auto it = map.find(binding);
            return it != map.end() ? it->second : binding;
        }
    };

    struct MetalMSLSlotLayout {
        MetalMSLSetSlots set;               // slots belonging to the requested set
        uint32_t         bufferCount  = 0;  // total buffer-category slots across all sets
        uint32_t         textureCount = 0;
        uint32_t         samplerCount = 0;

        // push constants are lowered to a plain constant buffer declared last,
        // so they occupy the highest buffer-category slot
        uint32_t PushConstantSlot() const
        {
            return bufferCount > 0 ? bufferCount - 1 : 0;
        }
    };

    inline bool IsMetalBufferResource(ShaderResourceType type)
    {
        return type == ShaderResourceType::UNIFORM_BUFFER || type == ShaderResourceType::STORAGE_BUFFER ||
               type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC || type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC;
    }

    inline bool IsMetalTextureResource(ShaderResourceType type)
    {
        return type == ShaderResourceType::SAMPLED_IMAGE || type == ShaderResourceType::STORAGE_IMAGE ||
               type == ShaderResourceType::INPUT_ATTACHMENT;
    }

    // Walks the whole reflection to keep the per-category counters aligned with
    // the generated MSL, recording the slots of `set` only.
    inline MetalMSLSlotLayout BuildMetalMSLSlotLayout(const ShaderReflection &reflection, uint32_t set)
    {
        MetalMSLSlotLayout layout;
        uint32_t           bufferIdx  = 0;
        uint32_t           textureIdx = 0;
        uint32_t           samplerIdx = 0;

        for (const auto &res : reflection.resources) {
            const bool     inSet = res.set == set;
            const uint32_t count = res.count > 0 ? res.count : 1;

            if (IsMetalBufferResource(res.type)) {
                if (inSet) {
                    for (uint32_t i = 0; i < count; ++i) {
                        layout.set.buffer[res.binding + i] = bufferIdx + i;
                    }
                }
                bufferIdx += count;
            } else if (IsMetalTextureResource(res.type)) {
                if (inSet) {
                    for (uint32_t i = 0; i < count; ++i) {
                        layout.set.texture[res.binding + i] = textureIdx + i;
                    }
                }
                textureIdx += count;
            } else if (res.type == ShaderResourceType::SAMPLER) {
                if (inSet) {
                    for (uint32_t i = 0; i < count; ++i) {
                        layout.set.sampler[res.binding + i] = samplerIdx + i;
                    }
                }
                samplerIdx += count;
            }
        }

        layout.bufferCount  = bufferIdx;
        layout.textureCount = textureIdx;
        layout.samplerCount = samplerIdx;
        return layout;
    }

} // namespace sky::aurora
