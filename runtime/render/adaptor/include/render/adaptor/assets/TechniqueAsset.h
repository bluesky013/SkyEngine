//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

#include <framework/asset/AssetManager.h>
#include <render/resource/Technique.h>
#include <render/RenderBase.h>

#include <rhi/Core.h>
#include <string>
#include <vector>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    enum class TechAssetType : uint32_t {
        GRAPHIC,
        MESH,
        COMPUTE
    };

    struct ShaderRefData {
        std::string shader;
        std::string objectOrCSMain;
        std::string vertOrMeshMain;
        std::string fragmentMain;
    };

    struct TechniqueVertexFlags {
        RenderVertexFlags flag;
        std::string macro;
    };

    struct TechniqueAssetData {
        uint32_t version;
        ShaderRefData shader;
        std::string passTag;
        TechAssetType type;

        rhi::DepthStencil            depthStencil;
        rhi::RasterState             rasterState;
        std::vector<rhi::BlendState> blendStates;

        std::vector<TechniqueVertexFlags> vertexFlags;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    template <>
    struct AssetTraits<Technique> {
        using DataType                                = TechniqueAssetData;
        static constexpr std::string_view ASSET_TYPE  = "Technique";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using TechniqueAssetPtr = std::shared_ptr<Asset<Technique>>;

    CounterPtr<Technique> CreateTechniqueFromAsset(const TechniqueAssetPtr &asset);
    CounterPtr<GraphicsTechnique> GreateGfxTechFromAsset(const TechniqueAssetPtr &asset);
}
