//
// RgBlockDesc implementation: std140-like layout table + RHI layout conversion.
//

#include <aurora/shader/RgBlockDesc.h>

namespace sky::aurora {

    namespace {
        struct FieldSizeAlign {
            uint32_t size;
            uint32_t align;
        };

        FieldSizeAlign SizeAlignOf(RgFieldType type)
        {
            switch (type) {
            case RgFieldType::FLOAT:  return {4, 4};
            case RgFieldType::FLOAT2: return {8, 8};
            case RgFieldType::FLOAT3: return {16, 16}; // vec3 occupies 16B under std140
            case RgFieldType::FLOAT4: return {16, 16};
            case RgFieldType::MAT4:   return {64, 16};
            case RgFieldType::INT:    return {4, 4};
            case RgFieldType::UINT:   return {4, 4};
            case RgFieldType::BOOL:   return {4, 4};
            default:                  return {0, 0};   // textures/samplers are not cbuffer fields
            }
        }
    } // namespace

    std::vector<RgFieldLayout> ComputeFieldOffsets(const RgBlockDesc &desc, uint32_t &totalSize)
    {
        std::vector<RgFieldLayout> layouts;
        layouts.reserve(desc.fields.size());

        uint32_t offset = 0;
        for (const auto &field : desc.fields) {
            auto sa = SizeAlignOf(field.type);
            if (sa.size == 0) {
                layouts.push_back({0, 0});
                continue;
            }
            if (field.arrayCount) {
                sa.align            = 16;
                const uint64_t size = uint64_t((sa.size + 15u) / 16u * 16u) * field.arrayCount;
                if (size > UINT32_MAX - 15u - offset) {
                    totalSize = 0;
                    return {};
                }
                sa.size = static_cast<uint32_t>(size);
            }
            offset = (offset + sa.align - 1u) / sa.align * sa.align;
            layouts.push_back({offset, sa.size});
            offset += sa.size;
        }
        totalSize = (offset + 15u) / 16u * 16u; // block size 16B aligned
        return layouts;
    }

} // namespace sky::aurora
