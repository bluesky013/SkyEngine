//
// Aurora material asset: versioned CPU payload of a Material's techniques
// (tag + ShaderRef + material render state) and typed property values
// (uniform bytes / texture Uuid). Runtime-only load (no offline build).
//

#pragma once

#include <aurora/resource/Material.h>
#include <aurora/shader/ShaderRef.h>
#include <core/util/Uuid.h>
#include <framework/asset/Asset.h>
#include <framework/serialization/BinaryArchive.h>

#include <cstdint>
#include <string>
#include <string_view>
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

        // Shared { name, type, value-bytes } codec (material properties / instance overrides).
        inline void SaveTypedValue(BinaryOutputArchive &ar, const std::string &name, uint32_t type, const std::vector<uint8_t> &value)
        {
            ar.SaveValue(name);
            ar.SaveValue(type);
            ar.SaveValue(static_cast<uint32_t>(value.size()));
            if (!value.empty()) {
                ar.SaveValue(reinterpret_cast<const char *>(value.data()), value.size());
            }
        }

        inline void LoadTypedValue(BinaryInputArchive &ar, std::string &name, uint32_t &type, std::vector<uint8_t> &value)
        {
            ar.LoadValue(name);
            ar.LoadValue(type);
            uint32_t size = 0;
            ar.LoadValue(size);
            value.resize(size);
            if (size != 0) {
                ar.LoadValue(reinterpret_cast<char *>(value.data()), size);
            }
        }

    } // namespace detail

    struct MaterialTechniqueData {
        std::string         tag;
        ShaderRef           shader;
        MaterialRenderState renderState;

        void Save(BinaryOutputArchive &ar) const
        {
            ar.SaveValue(tag);
            ar.SaveValue(shader.relativePath);
            ar.SaveValue(shader.entry);
            detail::SaveRenderState(ar, renderState);
        }

        void Load(BinaryInputArchive &ar)
        {
            ar.LoadValue(tag);
            ar.LoadValue(shader.relativePath);
            ar.LoadValue(shader.entry);
            detail::LoadRenderState(ar, renderState);
        }
    };

    struct MaterialPropertyData {
        std::string          name;
        MaterialPropertyType type = MaterialPropertyType::FLOAT;
        std::vector<uint8_t> value;   // uniform bytes; size implied by type
        Uuid                 texture; // texture binding (when type is a texture kind)

        void Save(BinaryOutputArchive &ar) const
        {
            detail::SaveTypedValue(ar, name, static_cast<uint32_t>(type), value);
            ar.SaveValue(texture);
        }

        void Load(BinaryInputArchive &ar)
        {
            uint32_t typeValue = 0;
            detail::LoadTypedValue(ar, name, typeValue, value);
            type = static_cast<MaterialPropertyType>(typeValue);
            ar.LoadValue(texture);
        }
    };

    struct MaterialAssetData {
        static constexpr uint32_t CURRENT_VERSION = 1;

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
            if (version != CURRENT_VERSION) {
                clear();
                return;
            }
            uint32_t techniqueCount = 0;
            ar.LoadValue(techniqueCount);
            techniques.resize(techniqueCount);
            for (auto &technique : techniques) {
                technique.Load(ar);
            }
            uint32_t propertyCount = 0;
            ar.LoadValue(propertyCount);
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
