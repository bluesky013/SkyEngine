//
// RgBlockDesc: single source of truth for a resource block (set/binding/fields).
// One description feeds both the RHI layout and the shader header generator.
//
// Platform mapping convention:
//   Vulkan : set -> VkDescriptorSetLayout, binding direct
//   DX12   : set -> root parameter index (one descriptor table per set;
//            small cbuffers may become root CBVs)
//   Metal  : set -> argument buffer index ([[buffer(N)]], Metal 3+);
//            (set,binding) -> MSL index remap table comes from SPIRV-Cross
//

#pragma once

#include <core/name/Name.h>
#include <aurora/rhi/Core.h>
#include <aurora/rhi/ResourceGroup.h>

#include <string>
#include <vector>

namespace sky::aurora {

    enum class RgFieldType : uint8_t {
        FLOAT = 0,
        FLOAT2,
        FLOAT3,
        FLOAT4,
        MAT4,
        TEXTURE2D,
        TEXTURE_CUBE,
        SAMPLER,
    };

    struct RgField {
        RgFieldType type;
        Name        name;
    };

    enum class RgBlockKind : uint8_t {
        CBUFFER = 0,      // uniform block with fields
        CBUFFER_DYNAMIC,  // dynamic uniform block (dynamic offset)
        RESOURCE,         // standalone texture/sampler (fields unused)
    };

    struct RgBlockDesc {
        uint32_t           set     = 0;
        uint32_t           binding = 0;
        Name               blockName;
        RgBlockKind        kind    = RgBlockKind::CBUFFER;
        ShaderStageFlags   stages  = ShaderStageFlagBit::VS | ShaderStageFlagBit::FS | ShaderStageFlagBit::CS;
        std::vector<RgField> fields;
    };

    // ---- std140-style size/alignment table (scalar/vec/mat4 only) ----
    struct RgFieldLayout {
        uint32_t offset;
        uint32_t size;
    };

    // compute per-field offsets and total block size (std140-like; vec3 occupies 16B)
    std::vector<RgFieldLayout> ComputeFieldOffsets(const RgBlockDesc &desc, uint32_t &totalSize);

    // RgBlockDesc -> ResourceGroupLayout::Descriptor (single binding entry)
    ResourceGroupLayout::Descriptor ToLayoutDescriptor(const RgBlockDesc &desc);

} // namespace sky::aurora
