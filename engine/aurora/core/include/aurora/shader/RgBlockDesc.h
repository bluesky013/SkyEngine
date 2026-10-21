//
// Shared declaration metadata and CPU field-layout validation.
// Native resource mappings come from backend shader reflection.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <core/name/Name.h>

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
        INT,
        UINT,
        BOOL,
    };

    struct RgField {
        RgFieldType type;
        Name        name;
        uint32_t    arrayCount = 0;
    };

    enum class RgBlockKind : uint8_t {
        CBUFFER = 0,     // uniform block with fields
        CBUFFER_DYNAMIC, // dynamic uniform block (dynamic offset)
        RESOURCE,        // standalone texture/sampler (fields unused)
    };

    struct RgBlockDesc {
        uint32_t             set     = 0;
        uint32_t             binding = 0;
        Name                 blockName;
        RgBlockKind          kind   = RgBlockKind::CBUFFER;
        ShaderStageFlags     stages = ShaderStageFlagBit::VS | ShaderStageFlagBit::FS | ShaderStageFlagBit::CS;
        std::vector<RgField> fields;
    };

    // ---- std140-style size/alignment table (scalar/vec/mat4 only) ----
    struct RgFieldLayout {
        uint32_t offset;
        uint32_t size;
    };

    // compute per-field offsets and total block size (std140-like; vec3 occupies 16B)
    std::vector<RgFieldLayout> ComputeFieldOffsets(const RgBlockDesc &desc, uint32_t &totalSize);

} // namespace sky::aurora
