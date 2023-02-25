//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

#include <rhi/Core.h>
#include <framework/asset/AssetManager.h>
#include <render/assets/Shader.h>
#include <string>
#include <vector>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct TechniqueAssetData {
        std::vector<ShaderAssetPtr>  shaders;
        std::string                  rasterTag;
        rhi::DepthStencil            depthStencil;
        rhi::RasterState             rasterState;
        std::vector<rhi::BlendState> blendStates;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    class Technique {
        Technique() = default;
        ~Technique() = default;
    };

    template <>
    struct AssetTraits<Technique> {
        using DataType                                = TechniqueAssetData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("79F513A7-8BC1-48B4-B086-FB2E78798D60");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using TechniqueAssetPtr = std::shared_ptr<Asset<Technique>>;
}