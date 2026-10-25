#include <aurora/resource/MaterialSchema.h>

#include <algorithm>

namespace sky::aurora {
    bool BuildMaterialSchema(const ShaderReflection &reflection, const ShaderVariantSchema &options, MaterialSchema &out, std::string &error)
    {
        ShaderParameterTable table;
        if (!BuildShaderParameterTable(reflection, 1, {}, table, error) || !options.Validate(&error))
            return false;
        MaterialSchema candidate;
        uint32_t       buffers = 0;
        const auto     fail    = [&](const std::string &message) {
            error = message;
            return false;
        };
        for (const auto &resource : reflection.declarations) {
            if (resource.set != 2)
                continue;
            if (resource.count != 1 || resource.descriptorArray)
                return fail("Material descriptor arrays are unsupported: " + resource.name);
            if (resource.type == ShaderResourceType::UNIFORM_BUFFER || resource.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC) {
                if (++buffers > 1)
                    return fail("A Material program family permits at most one logical Group 2 cbuffer");
                for (const auto &entry : table.entries) {
                    if (entry.set != 2 || entry.binding != resource.binding || entry.kind != ShaderParameterKind::FIELD)
                        continue;
                    if (entry.dataType == ShaderDataType::UNKNOWN || entry.dataType == ShaderDataType::FLOAT4_ARRAY)
                        return fail("Unsupported material field shape: " + entry.name);
                    candidate.keys.push_back({.name       = entry.name,
                                              .owner      = resource.declarationOwner,
                                              .kind       = MaterialKeyKind::FIELD,
                                              .dataType   = entry.dataType,
                                              .arrayCount = entry.arrayCount});
                }
            } else if (resource.type == ShaderResourceType::SAMPLED_IMAGE) {
                if (resource.texture.dimension != ShaderTextureDimension::D2 || resource.texture.scalarType != ShaderScalarType::FLOAT ||
                    resource.texture.components != 4 || resource.texture.arrayed || resource.texture.multisampled)
                    return fail("Material textures require ordinary float4 Texture2D declarations: " + resource.name);
                candidate.keys.push_back(
                    {.name = resource.name, .owner = resource.declarationOwner, .kind = MaterialKeyKind::TEXTURE, .texture = resource.texture});
            } else if (resource.type == ShaderResourceType::SAMPLER && resource.sampler == ShaderSamplerType::REGULAR) {
                candidate.keys.push_back(
                    {.name = resource.name, .owner = resource.declarationOwner, .kind = MaterialKeyKind::SAMPLER, .sampler = resource.sampler});
            } else {
                return fail("Unsupported material resource: " + resource.name);
            }
        }
        for (const auto &entry : options.entries)
            if (entry.domain == ShaderVariantDomain::SHADER)
                candidate.keys.push_back({.name          = std::string(entry.key.GetStr()),
                                          .kind          = MaterialKeyKind::OPTION,
                                          .optionType    = entry.type,
                                          .optionWidth   = entry.bitWidth,
                                          .optionDefault = entry.defaultValue});
        std::ranges::sort(candidate.keys, {}, &MaterialKey::name);
        for (size_t i = 1; i < candidate.keys.size(); ++i)
            if (candidate.keys[i - 1].name == candidate.keys[i].name)
                return fail("Material parameter and option keys collide: " + candidate.keys[i].name);
        out = std::move(candidate);
        return true;
    }

    bool MergeMaterialSchemas(const MaterialSchema &input, MaterialSchema &output, std::string &error)
    {
        auto candidate = output;
        for (const auto &key : input.keys) {
            const auto found = std::ranges::find(candidate.keys, key.name, &MaterialKey::name);
            if (found == candidate.keys.end())
                candidate.keys.push_back(key);
            else if (*found != key) {
                error = "Material techniques disagree on logical key: " + key.name;
                return false;
            }
        }
        std::ranges::sort(candidate.keys, {}, &MaterialKey::name);
        output = std::move(candidate);
        return true;
    }
} // namespace sky::aurora
