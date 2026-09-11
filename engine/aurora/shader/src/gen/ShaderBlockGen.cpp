//
// ShaderBlockGen implementation.
//

#include <aurora/shader/gen/ShaderBlockGen.h>

#include <core/hash/Fnv1a.h>

#include <sstream>

namespace sky::aurora {

    namespace {
        const char *HlslTypeName(RgFieldType type)
        {
            switch (type) {
            case RgFieldType::FLOAT:  return "float";
            case RgFieldType::FLOAT2: return "float2";
            case RgFieldType::FLOAT3: return "float3";
            case RgFieldType::FLOAT4: return "float4";
            case RgFieldType::MAT4:   return "float4x4";
            default:                  return "float4";
            }
        }

        char RegisterPrefix(RgFieldType type)
        {
            switch (type) {
            case RgFieldType::TEXTURE2D:
            case RgFieldType::TEXTURE_CUBE: return 't';
            case RgFieldType::SAMPLER:      return 's';
            default:                        return 'b';
            }
        }

        const char *HlslResourceType(RgFieldType type)
        {
            switch (type) {
            case RgFieldType::TEXTURE2D:   return "Texture2D";
            case RgFieldType::TEXTURE_CUBE: return "TextureCube";
            case RgFieldType::SAMPLER:      return "SamplerState";
            default:                        return nullptr;
            }
        }
    } // namespace

    std::string ShaderBlockGen::GenerateHlsl(const RgBlockDesc &desc)
    {
        std::ostringstream ss;
        const std::string blockName = desc.blockName.GetStr().data();

        if (desc.kind == RgBlockKind::RESOURCE) {
            // standalone texture/sampler declaration
            const RgFieldType type = desc.fields.empty() ? RgFieldType::TEXTURE2D : desc.fields.front().type;
            const char *resourceType = HlslResourceType(type);
            ss << "[[vk::binding(" << desc.binding << ", " << desc.set << ")]] "
               << resourceType << " " << blockName << " : register("
               << RegisterPrefix(type) << desc.binding << ", space" << desc.set << ");\n";
            return ss.str();
        }

        ss << "[[vk::binding(" << desc.binding << ", " << desc.set << ")]] "
           << "cbuffer " << blockName << " : register(b" << desc.binding
           << ", space" << desc.set << ")\n{\n";
        for (const auto &field : desc.fields) {
            ss << "    " << HlslTypeName(field.type) << " "
               << field.name.GetStr().data() << ";\n";
        }
        ss << "};\n";
        return ss.str();
    }

    uint32_t ShaderBlockGen::ContentHash(const RgBlockDesc &desc)
    {
        return Fnv1a32(GenerateHlsl(desc));
    }

} // namespace sky::aurora
