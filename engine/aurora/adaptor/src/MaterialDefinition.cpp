//
// Created on 2026/11/15.
//

#include <aurora/adaptor/assets/MaterialAsset.h>

#include <algorithm>
#include <aurora/shader/ShaderResolver.h>
#include <bit>
#include <map>

namespace sky::aurora {
    bool AppendMaterialShaderOptions(const ShaderVariantSchema            &schema,
                                     std::span<const MaterialPropertyData> properties,
                                     ShaderVariant                        &values,
                                     std::string                          &error)
    {
        auto candidate = values;
        for (const auto &property : properties) {
            const auto *option = schema.FindEntry(Name(property.name.c_str()));
            if (!option || option->domain != ShaderVariantDomain::SHADER)
                continue;
            ShaderVariantEntry value{option->key};
            if (const auto *v = std::get_if<bool>(&property.value)) {
                value.type  = ShaderVariantType::BOOL;
                value.value = *v ? 1u : 0u;
            } else if (const auto *v = std::get_if<int32_t>(&property.value)) {
                value.type  = ShaderVariantType::INT32;
                value.value = std::bit_cast<uint32_t>(*v);
            } else if (const auto *v = std::get_if<uint32_t>(&property.value)) {
                value.value = *v;
            } else {
                error = "Material option requires bool/int32/uint32: " + property.name;
                return false;
            }
            if (!MaterialPropertyValueMatches(property.type, property.value) || !ValidateShaderVariantValue(*option, value)) {
                error = "Material option type or value does not match its declaration: " + property.name;
                return false;
            }
            candidate.entries.push_back(value);
        }
        values = std::move(candidate);
        return true;
    }

    std::optional<MaterialPropertyData> MaterialDefaultValue(const MaterialKey &key)
    {
        MaterialPropertyData property;
        property.name = key.name;
        if (key.kind == MaterialKeyKind::OPTION) {
            if (key.optionType == ShaderVariantType::BOOL)
                property.value = key.optionDefault != 0;
            else if (key.optionType == ShaderVariantType::INT32)
                property.value = std::bit_cast<int32_t>(key.optionDefault);
            else
                property.value = key.optionDefault;
        } else if (key.kind == MaterialKeyKind::TEXTURE) {
            property.value = Uuid{};
        } else if (key.kind == MaterialKeyKind::SAMPLER) {
            property.value = Sampler::Descriptor{};
        } else {
            switch (key.dataType) {
            case ShaderDataType::FLOAT: property.value = 0.f; break;
            case ShaderDataType::FLOAT2: property.value = Vector2{}; break;
            case ShaderDataType::FLOAT4: property.value = Vector4{}; break;
            case ShaderDataType::MAT4: property.value = Matrix4{}; break;
            case ShaderDataType::INT: property.value = int32_t{}; break;
            case ShaderDataType::UINT: property.value = uint32_t{}; break;
            default: return {};
            }
        }
        property.type = std::visit(
            [](const auto &value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, Uuid>)
                    return MaterialPropertyType::TEXTURE2D;
                else if constexpr (std::is_same_v<T, Sampler::Descriptor>)
                    return MaterialPropertyType::SAMPLER;
                else
                    return MaterialPropertyTypeOf<T>();
            },
            property.value);
        return property;
    }

    bool ResolveMaterialDefinition(
        const MaterialAssetData &source, ShaderFileSystem &files, API backend, IFileSystem *products, MaterialDefinitionData &out, std::string &error)
    {
        if (!ValidateMaterialAssetData(source, error))
            return false;
        MaterialDefinitionData candidate;
        candidate.canonical = source;
        const auto fail     = [&](std::string message) {
            error = std::move(message);
            return false;
        };
        const auto     target = backend == API::VULKAN ? ShaderTarget::SPIRV : backend == API::DX12 ? ShaderTarget::DXIL : ShaderTarget::MSL;
        ShaderResolver resolver(products, nullptr, files, 0, 0, products ? ShaderResolveMode::PRODUCTS : ShaderResolveMode::DEVELOPMENT);
        std::map<std::string, std::string>        aliases;
        std::map<std::string, ShaderVariantEntry> legacyOptions;
        for (const auto &authored : source.techniques) {
            ShaderVariantSchema schema;
            for (const auto &stage : {authored.program.vertex, authored.program.fragment}) {
                ShaderVariantSchema stageSchema;
                uint64_t            fingerprint = 0;
                if (!resolver.ResolveSchema(stage.relativePath, static_cast<uint32_t>(target), stageSchema, fingerprint, &error) ||
                    !MergeStageVariantSchemas(schema, stageSchema, schema, error))
                    return false;
            }
            auto program = authored.program;
            for (const auto &value : program.variant.entries) {
                const std::string key(value.key.GetStr());
                const auto [existing, inserted] = legacyOptions.emplace(key, value);
                if (!inserted && existing->second != value)
                    return fail("Legacy techniques disagree on a Material option: " + key);
            }
            if (!AppendMaterialShaderOptions(schema, source.properties, program.variant, error))
                return false;
            auto compiled = CompileShaderTechnique(files, backend, program, products);
            if (const auto *failure = std::get_if<RendererError>(&compiled))
                return fail(failure->message);
            const auto    &prepared = std::get<RenderTechniqueData>(compiled);
            MaterialSchema keys;
            if (!BuildMaterialSchema(prepared.reflection, prepared.variantSchema, keys, error) ||
                !MergeMaterialSchemas(keys, candidate.schema, error))
                return false;
            for (const auto &alias : authored.aliases) {
                const auto key = std::ranges::find(keys.keys, alias.target, &MaterialKey::name);
                if (key == keys.keys.end() || key->kind == MaterialKeyKind::OPTION)
                    return fail("Legacy alias has no exact Group 2 declaration: " + alias.target);
                const auto [entry, inserted] = aliases.emplace(alias.alias, alias.target);
                if (!inserted && entry->second != alias.target)
                    return fail("Legacy aliases disagree: " + alias.alias);
            }
        }
        for (const auto &[name, value] : legacyOptions) {
            const auto key = std::ranges::find(candidate.schema.keys, name, &MaterialKey::name);
            if (key == candidate.schema.keys.end() || key->kind != MaterialKeyKind::OPTION)
                return fail("Legacy technique option has no Shader declaration: " + name);
            auto property = MaterialDefaultValue(*key);
            if (value.type == ShaderVariantType::BOOL)
                property->value = value.value != 0;
            else if (value.type == ShaderVariantType::INT32)
                property->value = std::bit_cast<int32_t>(value.value);
            else
                property->value = value.value;
            candidate.canonical.properties.push_back(std::move(*property));
        }
        std::set<std::string> names;
        for (auto &property : candidate.canonical.properties) {
            if (const auto name = aliases.find(property.name); name != aliases.end())
                property.name = name->second;
            const auto key = std::ranges::find(candidate.schema.keys, property.name, &MaterialKey::name);
            if (key == candidate.schema.keys.end() || !names.insert(property.name).second)
                return fail("Unknown or conflicting material key: " + property.name);
            const auto expected = MaterialDefaultValue(*key);
            if (!expected || expected->type != property.type || !MaterialPropertyValueMatches(property.type, property.value))
                return fail("Material value has the wrong logical type: " + property.name);
        }
        for (auto &technique : candidate.canonical.techniques) {
            technique.aliases.clear();
            technique.program.variant.entries.clear();
        }
        out = std::move(candidate);
        return true;
    }

} // namespace sky::aurora
