//
// Versioned material values use declaration keys; legacy aliases are resolved on import.
//

#pragma once

#include <aurora/resource/Material.h>
#include <aurora/resource/MaterialSchema.h>
#include <aurora/shader/ShaderRef.h>
#include <core/math/Matrix4.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/util/Uuid.h>
#include <framework/asset/Asset.h>
#include <framework/serialization/BinaryArchive.h>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sky::aurora {

    class Device;

    namespace detail {

        inline void SaveStencilState(BinaryOutputArchive &ar, const StencilState &s)
        {
            ar.SaveValue(static_cast<uint32_t>(s.failOp));
            ar.SaveValue(static_cast<uint32_t>(s.passOp));
            ar.SaveValue(static_cast<uint32_t>(s.depthFailOp));
            ar.SaveValue(static_cast<uint32_t>(s.compareOp));
            ar.SaveValue(s.compareMask);
            ar.SaveValue(s.writeMask);
            ar.SaveValue(s.reference);
        }

        inline void LoadStencilState(BinaryInputArchive &ar, StencilState &s)
        {
            uint32_t failOp      = 0;
            uint32_t passOp      = 0;
            uint32_t depthFailOp = 0;
            uint32_t compareOp   = 0;
            ar.LoadValue(failOp);
            ar.LoadValue(passOp);
            ar.LoadValue(depthFailOp);
            ar.LoadValue(compareOp);
            s.failOp      = static_cast<StencilOp>(failOp);
            s.passOp      = static_cast<StencilOp>(passOp);
            s.depthFailOp = static_cast<StencilOp>(depthFailOp);
            s.compareOp   = static_cast<CompareOp>(compareOp);
            ar.LoadValue(s.compareMask);
            ar.LoadValue(s.writeMask);
            ar.LoadValue(s.reference);
        }

        inline void SaveBlendState(BinaryOutputArchive &ar, const BlendState &s)
        {
            ar.SaveValue(s.blendEn);
            ar.SaveValue(s.writeMask);
            ar.SaveValue(static_cast<uint32_t>(s.srcColor));
            ar.SaveValue(static_cast<uint32_t>(s.dstColor));
            ar.SaveValue(static_cast<uint32_t>(s.srcAlpha));
            ar.SaveValue(static_cast<uint32_t>(s.dstAlpha));
            ar.SaveValue(static_cast<uint32_t>(s.colorBlendOp));
            ar.SaveValue(static_cast<uint32_t>(s.alphaBlendOp));
        }

        inline void LoadBlendState(BinaryInputArchive &ar, BlendState &s)
        {
            uint32_t srcColor     = 0;
            uint32_t dstColor     = 0;
            uint32_t srcAlpha     = 0;
            uint32_t dstAlpha     = 0;
            uint32_t colorBlendOp = 0;
            uint32_t alphaBlendOp = 0;
            ar.LoadValue(s.blendEn);
            ar.LoadValue(s.writeMask);
            ar.LoadValue(srcColor);
            ar.LoadValue(dstColor);
            ar.LoadValue(srcAlpha);
            ar.LoadValue(dstAlpha);
            ar.LoadValue(colorBlendOp);
            ar.LoadValue(alphaBlendOp);
            s.srcColor     = static_cast<BlendFactor>(srcColor);
            s.dstColor     = static_cast<BlendFactor>(dstColor);
            s.srcAlpha     = static_cast<BlendFactor>(srcAlpha);
            s.dstAlpha     = static_cast<BlendFactor>(dstAlpha);
            s.colorBlendOp = static_cast<BlendOp>(colorBlendOp);
            s.alphaBlendOp = static_cast<BlendOp>(alphaBlendOp);
        }

        inline void SaveRenderState(BinaryOutputArchive &ar, const MaterialRenderState &rs)
        {
            // DepthStencil (field-wise: the structs carry padding / bools and
            // must not be raw-copied).
            ar.SaveValue(rs.depthStencil.depthTest);
            ar.SaveValue(rs.depthStencil.depthWrite);
            ar.SaveValue(rs.depthStencil.stencilTest);
            ar.SaveValue(rs.depthStencil.rsv);
            ar.SaveValue(static_cast<uint32_t>(rs.depthStencil.compareOp));
            ar.SaveValue(rs.depthStencil.minDepth);
            ar.SaveValue(rs.depthStencil.maxDepth);
            SaveStencilState(ar, rs.depthStencil.front);
            SaveStencilState(ar, rs.depthStencil.back);

            // RasterState
            ar.SaveValue(rs.rasterState.depthClampEnable);
            ar.SaveValue(rs.rasterState.rasterizerDiscardEnable);
            ar.SaveValue(rs.rasterState.depthBiasEnable);
            ar.SaveValue(rs.rasterState.depthClipEnable);
            ar.SaveValue(rs.rasterState.depthBiasConstantFactor);
            ar.SaveValue(rs.rasterState.depthBiasClamp);
            ar.SaveValue(rs.rasterState.depthBiasSlopeFactor);
            ar.SaveValue(rs.rasterState.lineWidth);
            ar.SaveValue(rs.rasterState.cullMode);
            ar.SaveValue(static_cast<uint32_t>(rs.rasterState.frontFace));
            ar.SaveValue(static_cast<uint32_t>(rs.rasterState.polygonMode));

            // Blend states (per color target)
            ar.SaveValue(static_cast<uint32_t>(rs.blendStates.size()));
            for (const auto &blend : rs.blendStates) {
                SaveBlendState(ar, blend);
            }

            ar.SaveValue(rs.alphaToCoverage);
        }

        inline void LoadRenderState(BinaryInputArchive &ar, MaterialRenderState &rs)
        {
            uint32_t compareOp = 0;
            ar.LoadValue(rs.depthStencil.depthTest);
            ar.LoadValue(rs.depthStencil.depthWrite);
            ar.LoadValue(rs.depthStencil.stencilTest);
            ar.LoadValue(rs.depthStencil.rsv);
            ar.LoadValue(compareOp);
            rs.depthStencil.compareOp = static_cast<CompareOp>(compareOp);
            ar.LoadValue(rs.depthStencil.minDepth);
            ar.LoadValue(rs.depthStencil.maxDepth);
            LoadStencilState(ar, rs.depthStencil.front);
            LoadStencilState(ar, rs.depthStencil.back);

            ar.LoadValue(rs.rasterState.depthClampEnable);
            ar.LoadValue(rs.rasterState.rasterizerDiscardEnable);
            ar.LoadValue(rs.rasterState.depthBiasEnable);
            ar.LoadValue(rs.rasterState.depthClipEnable);
            ar.LoadValue(rs.rasterState.depthBiasConstantFactor);
            ar.LoadValue(rs.rasterState.depthBiasClamp);
            ar.LoadValue(rs.rasterState.depthBiasSlopeFactor);
            ar.LoadValue(rs.rasterState.lineWidth);
            ar.LoadValue(rs.rasterState.cullMode);
            uint32_t frontFace   = 0;
            uint32_t polygonMode = 0;
            ar.LoadValue(frontFace);
            ar.LoadValue(polygonMode);
            rs.rasterState.frontFace   = static_cast<FrontFace>(frontFace);
            rs.rasterState.polygonMode = static_cast<PolygonMode>(polygonMode);

            uint32_t blendCount = 0;
            ar.LoadValue(blendCount);
            rs.blendStates.resize(blendCount);
            for (auto &blend : rs.blendStates) {
                LoadBlendState(ar, blend);
            }

            ar.LoadValue(rs.alphaToCoverage);
        }

        // Typed property value (memory-lean: one alternative active). Textures use the Uuid
        // alternative; the property's MaterialPropertyType tells which alternative / texture kind.
        using MaterialPropertyValue = std::variant<float, Vector2, Vector3, Vector4, Matrix4, int32_t, uint32_t, bool, Uuid, Sampler::Descriptor>;

        inline void SaveSampler(BinaryOutputArchive &ar, const Sampler::Descriptor &value)
        {
            ar.SaveValue(static_cast<uint32_t>(value.magFilter));
            ar.SaveValue(static_cast<uint32_t>(value.minFilter));
            ar.SaveValue(static_cast<uint32_t>(value.mipmapMode));
            ar.SaveValue(static_cast<uint32_t>(value.addressModeU));
            ar.SaveValue(static_cast<uint32_t>(value.addressModeV));
            ar.SaveValue(static_cast<uint32_t>(value.addressModeW));
            ar.SaveValue(value.minLod);
            ar.SaveValue(value.maxLod);
            ar.SaveValue(value.maxAnisotropy);
            ar.SaveValue(value.anisotropyEnable);
        }
        inline void LoadSampler(BinaryInputArchive &ar, Sampler::Descriptor &value)
        {
            uint32_t mag = 0, min = 0, mip = 0, u = 0, v = 0, w = 0;
            ar.LoadValue(mag);
            ar.LoadValue(min);
            ar.LoadValue(mip);
            ar.LoadValue(u);
            ar.LoadValue(v);
            ar.LoadValue(w);
            value.magFilter    = static_cast<Filter>(mag);
            value.minFilter    = static_cast<Filter>(min);
            value.mipmapMode   = static_cast<MipFilter>(mip);
            value.addressModeU = static_cast<WrapMode>(u);
            value.addressModeV = static_cast<WrapMode>(v);
            value.addressModeW = static_cast<WrapMode>(w);
            ar.LoadValue(value.minLod);
            ar.LoadValue(value.maxLod);
            ar.LoadValue(value.maxAnisotropy);
            ar.LoadValue(value.anisotropyEnable);
        }

        inline void SavePropertyValue(BinaryOutputArchive &ar, const MaterialPropertyValue &value)
        {
            ar.SaveValue(static_cast<uint32_t>(value.index()));
            std::visit(
                [&ar](const auto &v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, float>) {
                        ar.SaveValue(reinterpret_cast<const char *>(&v), sizeof(float));
                    } else if constexpr (std::is_same_v<T, Vector2>) {
                        ar.SaveValue(reinterpret_cast<const char *>(v.v), sizeof(float) * 2);
                    } else if constexpr (std::is_same_v<T, Vector3>) {
                        ar.SaveValue(reinterpret_cast<const char *>(v.v), sizeof(float) * 3);
                    } else if constexpr (std::is_same_v<T, Vector4>) {
                        ar.SaveValue(reinterpret_cast<const char *>(v.v), sizeof(float) * 4);
                    } else if constexpr (std::is_same_v<T, Matrix4>) {
                        ar.SaveValue(reinterpret_cast<const char *>(v.v), sizeof(float) * 16);
                    } else if constexpr (std::is_same_v<T, Sampler::Descriptor>) {
                        SaveSampler(ar, v);
                    } else if constexpr (std::is_same_v<T, Uuid>) {
                        ar.SaveValue(v);
                    } else {
                        ar.SaveValue(v);
                    }
                },
                value);
        }

        template <typename T>
        inline void LoadOne(BinaryInputArchive &ar, T &v)
        {
            if constexpr (std::is_same_v<T, float>) {
                ar.LoadValue(reinterpret_cast<char *>(&v), sizeof(float));
            } else if constexpr (std::is_same_v<T, Vector2>) {
                ar.LoadValue(reinterpret_cast<char *>(v.v), sizeof(float) * 2);
            } else if constexpr (std::is_same_v<T, Vector3>) {
                ar.LoadValue(reinterpret_cast<char *>(v.v), sizeof(float) * 3);
            } else if constexpr (std::is_same_v<T, Vector4>) {
                ar.LoadValue(reinterpret_cast<char *>(v.v), sizeof(float) * 4);
            } else if constexpr (std::is_same_v<T, Matrix4>) {
                ar.LoadValue(reinterpret_cast<char *>(v.v), sizeof(float) * 16);
            } else if constexpr (std::is_same_v<T, Sampler::Descriptor>) {
                LoadSampler(ar, v);
            } else {
                ar.LoadValue(v);
            }
        }

        template <typename V, std::size_t I = 0>
        inline void LoadVariantByIndex(BinaryInputArchive &ar, uint32_t index, V &out)
        {
            if constexpr (I < std::variant_size_v<V>) {
                if (index == I) {
                    std::variant_alternative_t<I, V> alt{};
                    LoadOne(ar, alt);
                    out = std::move(alt);
                    return;
                }
                LoadVariantByIndex<V, I + 1>(ar, index, out);
            }
        }

        inline void LoadPropertyValue(BinaryInputArchive &ar, MaterialPropertyValue &value)
        {
            uint32_t index = 0;
            ar.LoadValue(index);
            LoadVariantByIndex(ar, index, value);
        }

    } // namespace detail

    using MaterialPropertyValue = detail::MaterialPropertyValue;

    struct MaterialTechniqueData {
        std::string                       tag;
        ShaderTechnique                   program;
        std::vector<ShaderParameterAlias> aliases;
        MaterialRenderState               renderState;

        void Save(BinaryOutputArchive &ar) const
        {
            ar.SaveValue(tag);
            for (const auto *ref : {&program.vertex, &program.fragment, &program.compute}) {
                ar.SaveValue(ref->relativePath);
                ar.SaveValue(ref->entry);
            }
            ar.SaveValue(static_cast<uint32_t>(program.variant.entries.size()));
            for (const auto &entry : program.variant.entries) {
                ar.SaveValue(std::string(entry.key.GetStr()));
                ar.SaveValue(entry.value);
                ar.SaveValue(static_cast<uint32_t>(entry.type));
            }
            detail::SaveRenderState(ar, renderState);
            ar.SaveValue(static_cast<uint32_t>(aliases.size()));
            for (const auto &alias : aliases) {
                ar.SaveValue(alias.alias);
                ar.SaveValue(alias.target);
            }
        }

        void Load(BinaryInputArchive &ar, bool legacy = false)
        {
            ar.LoadValue(tag);
            for (auto *ref : {&program.vertex, &program.fragment, &program.compute}) {
                ar.LoadValue(ref->relativePath);
                ar.LoadValue(ref->entry);
            }
            uint32_t count = 0;
            ar.LoadValue(count);
            if (count > 1024) {
                ar.Invalidate();
                return;
            }
            program.variant.entries.resize(count);
            for (auto &entry : program.variant.entries) {
                std::string name;
                ar.LoadValue(name);
                entry.key = Name(name.c_str());
                ar.LoadValue(entry.value);
                uint32_t type = 0;
                if (!legacy)
                    ar.LoadValue(type);
                if (type > static_cast<uint32_t>(ShaderVariantType::BOOL) ||
                    (type == static_cast<uint32_t>(ShaderVariantType::BOOL) && entry.value > 1)) {
                    ar.Invalidate();
                    return;
                }
                entry.type = static_cast<ShaderVariantType>(type);
            }
            detail::LoadRenderState(ar, renderState);
            ar.LoadValue(count);
            if (count > 1024) {
                tag.clear();
                return;
            }
            aliases.resize(count);
            for (auto &alias : aliases) {
                ar.LoadValue(alias.alias);
                ar.LoadValue(alias.target);
            }
        }
    };

    struct MaterialPropertyData {
        std::string                   name;
        MaterialPropertyType          type = MaterialPropertyType::FLOAT;
        detail::MaterialPropertyValue value;

        void Save(BinaryOutputArchive &ar) const
        {
            ar.SaveValue(name);
            ar.SaveValue(static_cast<uint32_t>(type));
            detail::SavePropertyValue(ar, value);
        }

        void Load(BinaryInputArchive &ar)
        {
            uint32_t typeValue = 0;
            ar.LoadValue(name);
            ar.LoadValue(typeValue);
            type = static_cast<MaterialPropertyType>(typeValue);
            detail::LoadPropertyValue(ar, value);
        }
    };

    struct MaterialAssetData {
        static constexpr uint32_t CURRENT_VERSION = 4;

        uint32_t                           version = CURRENT_VERSION;
        std::vector<MaterialTechniqueData> techniques;
        std::vector<MaterialPropertyData>  properties;

        void Save(BinaryOutputArchive &ar) const
        {
            ar.SaveValue(version);
            ar.SaveValue(static_cast<uint32_t>(techniques.size()));
            for (const auto &technique : techniques) {
                technique.Save(ar);
            }
            ar.SaveValue(static_cast<uint32_t>(properties.size()));
            for (const auto &property : properties) {
                property.Save(ar);
            }
        }

        void Load(BinaryInputArchive &ar)
        {
            ar.LoadValue(version);
            if (version != CURRENT_VERSION && version != 3) {
                clear();
                ar.Invalidate();
                return;
            }
            const bool legacy       = version == 3;
            version                 = CURRENT_VERSION;
            uint32_t techniqueCount = 0;
            ar.LoadValue(techniqueCount);
            if (techniqueCount > 32) {
                ar.Invalidate();
                return;
            }
            techniques.resize(techniqueCount);
            for (auto &technique : techniques) {
                technique.Load(ar, legacy);
            }
            uint32_t propertyCount = 0;
            ar.LoadValue(propertyCount);
            if (propertyCount > 4096) {
                ar.Invalidate();
                return;
            }
            properties.resize(propertyCount);
            for (auto &property : properties) {
                property.Load(ar);
            }
        }

        void clear()
        {
            techniques.clear();
            properties.clear();
        }
    };

    inline bool MaterialPropertyValueMatches(MaterialPropertyType type, const MaterialPropertyValue &value)
    {
        return std::visit(
            [&](const auto &typed) {
                using T = std::decay_t<decltype(typed)>;
                if constexpr (std::is_same_v<T, Uuid>)
                    return IsTextureProperty(type);
                else if constexpr (std::is_same_v<T, Sampler::Descriptor>)
                    return type == MaterialPropertyType::SAMPLER && IsValidMaterialSampler(typed);
                else
                    return MaterialPropertyTypeOf<T>() == type;
            },
            value);
    }

    inline bool ValidateMaterialAssetData(const MaterialAssetData &data, std::string &error)
    {
        if (data.version != MaterialAssetData::CURRENT_VERSION || data.techniques.empty() || data.techniques.size() > 32 ||
            data.properties.size() > 1024) {
            error = "Material requires the current schema and bounded explicit techniques/properties";
            return false;
        }
        std::set<std::string> properties, tags;
        for (const auto &property : data.properties)
            if (property.name.empty() || !properties.insert(property.name).second || !MaterialPropertyValueMatches(property.type, property.value)) {
                error = "Material property names or semantic value types are invalid";
                return false;
            }
        for (const auto &technique : data.techniques) {
            if (technique.tag.empty() || !tags.insert(technique.tag).second || technique.program.vertex.relativePath.empty() ||
                technique.program.fragment.relativePath.empty() || !technique.program.compute.relativePath.empty() ||
                technique.aliases.size() > 1024) {
                error = "Material requires unique graphics techniques";
                return false;
            }
            std::set<std::string> aliases, targets;
            for (const auto &alias : technique.aliases)
                if (!properties.contains(alias.alias) || alias.target.empty() || !aliases.insert(alias.alias).second ||
                    !targets.insert(alias.target).second) {
                    error = "Material aliases require unique declared properties and canonical targets";
                    return false;
                }
        }
        return true;
    }

    struct MaterialDefinitionData {
        MaterialSchema    schema;
        MaterialAssetData canonical;
    };
    class ShaderFileSystem;
    bool AppendMaterialShaderOptions(const ShaderVariantSchema            &schema,
                                     std::span<const MaterialPropertyData> properties,
                                     ShaderVariant                        &values,
                                     std::string                          &error);
    bool ResolveMaterialDefinition(const MaterialAssetData &, ShaderFileSystem &, API, IFileSystem *, MaterialDefinitionData &, std::string &);
    std::optional<MaterialPropertyData> MaterialDefaultValue(const MaterialKey &);

    // Runtime-only build: maps the asset onto a Material (techniques with
    // ShaderRef + render state, and typed property defaults). Textures are
    // resolved through AssetManager + CreateTextureFromAsset; shaders are left
    // as ShaderRef for the renderer's ShaderResolver.
    CounterPtr<Material> CreateMaterialFromAsset(Device *device, const Asset<Material> &asset);

} // namespace sky::aurora

namespace sky {

    template <>
    struct AssetTraits<sky::aurora::Material> {
        using DataType                                   = sky::aurora::MaterialAssetData;
        static constexpr std::string_view ASSET_TYPE     = "AuroraMaterial";
        static constexpr SerializeType    SERIALIZE_TYPE = SerializeType::BIN;
    };

} // namespace sky
