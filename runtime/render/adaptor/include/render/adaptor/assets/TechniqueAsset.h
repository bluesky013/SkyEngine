//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

#include <framework/asset/AssetManager.h>
#include <render/resource/Technique.h>

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
        Uuid shader;
        std::string objectOrCSMain;
        std::string vertOrMeshMain;
        std::string fragmentMain;
    };

    struct TechniqueAssetData {
        uint32_t version;
        ShaderRefData shader;
        std::string passTag;
        std::string vertexDesc;
        TechAssetType type;

        rhi::DepthStencil            depthStencil;
        rhi::RasterState             rasterState;
        std::vector<rhi::BlendState> blendStates;

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
