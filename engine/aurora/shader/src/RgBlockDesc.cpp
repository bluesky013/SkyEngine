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
            const auto sa = SizeAlignOf(field.type);
            if (sa.size == 0) {
                layouts.push_back({0, 0});
                continue;
            }
            // align up
            offset = (offset + sa.align - 1u) / sa.align * sa.align;
            layouts.push_back({offset, sa.size});
            offset += sa.size;
        }
        totalSize = (offset + 15u) / 16u * 16u; // block size 16B aligned
        return layouts;
    }

    ResourceGroupLayout::Descriptor ToLayoutDescriptor(const RgBlockDesc &desc)
    {
        ResourceGroupLayout::Descriptor layout{};

        DescriptorType type = DescriptorType::UNIFORM_BUFFER;
        switch (desc.kind) {
        case RgBlockKind::CBUFFER:         type = DescriptorType::UNIFORM_BUFFER;         break;
        case RgBlockKind::CBUFFER_DYNAMIC: type = DescriptorType::UNIFORM_BUFFER_DYNAMIC; break;
        case RgBlockKind::RESOURCE:
            if (!desc.fields.empty()) {
                switch (desc.fields.front().type) {
                case RgFieldType::TEXTURE2D:
                case RgFieldType::TEXTURE_CUBE: type = DescriptorType::SAMPLED_IMAGE; break;
                case RgFieldType::SAMPLER:      type = DescriptorType::SAMPLER;       break;
                default: break;
                }
            }
            break;
        }

        ResourceGroupLayout::BindingDesc binding{};
        binding.binding = desc.binding;
        binding.type    = type;
        binding.count   = 1;
        binding.stages  = desc.stages;
        layout.bindings.push_back(binding);
        return layout;
    }

} // namespace sky::aurora
