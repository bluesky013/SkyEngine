//
// Created by Zach Lee on 2023/2/26.
//

#pragma once

#include <framework/asset/Asset.h>
#include <render/assets/Mesh.h>
#include <render/assets/Image.h>
#include <render/assets/Material.h>
#include <render/assets/Material.h>
#include <core/math/Matrix4.h>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct RenderPrefabNode {
        uint32_t meshIndex = ~(0u);
        uint32_t parentIndex = ~(0u);
        std::vector<uint32_t> children;
        Matrix4 localMatrix;
    };

    struct RenderPrefabAssetData {
        std::vector<RenderPrefabNode> nodes;
        std::vector<ImageAssetPtr> images;
        std::vector<MeshAssetPtr> meshes;

        void Load(BinaryInputArchive &archive) {}
        void Save(BinaryOutputArchive &archive) const {}
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