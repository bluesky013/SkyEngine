//
// Created by Zach Lee on 2023/2/26.
//

#pragma once

#include <core/math/Matrix4.h>
#include <framework/asset/Asset.h>
#include <render/adaptor/assets/ImageAsset.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/MeshAsset.h>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct RenderPrefabNode {
        uint32_t meshIndex = ~(0u);
        uint32_t parentIndex = ~(0u);
        Matrix4 localMatrix;
    };

    struct RenderPrefabAssetData {
        std::vector<ImageAssetPtr> images;
        std::vector<MeshAssetPtr> meshes;
        std::vector<MaterialAssetPtr> materials;
        std::vector<RenderPrefabNode> nodes;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    class RenderPrefab {
    public:
        RenderPrefab() = default;
        ~RenderPrefab() = default;
    };

    template <>
    struct AssetTraits<RenderPrefab> {
        using DataType                                = RenderPrefabAssetData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("0339389A-D8BC-46B4-97F2-60B5548A30D7");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using RenderPrefabAssetPtr = std::shared_ptr<Asset<RenderPrefab>>;
}