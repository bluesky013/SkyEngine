//
// Created by Zach Lee on 2023/2/26.
//

#pragma once

#include <core/math/Matrix4.h>
#include <core/math/Transform.h>
#include <framework/asset/Asset.h>
#include <render/adaptor/assets/ImageAsset.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/MeshAsset.h>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct RenderPrefabNode {
        std::string name;
        Uuid mesh;
        Uuid material;
        uint32_t parentIndex = ~(0u);
        Transform localTransform;
    };

    struct RenderPrefabAssetData {
        std::vector<RenderPrefabNode> nodes;

        void LoadBin(BinaryInputArchive &archive);
        void SaveBin(BinaryOutputArchive &archive) const;

        void LoadJson(JsonInputArchive &archive);
        void SaveJson(JsonOutputArchive &archive) const;
    };

    class RenderPrefab : public RefObject {
    public:
        RenderPrefab() = default;
        ~RenderPrefab() override = default;
    };

    template <>
    struct AssetTraits<RenderPrefab> {
        using DataType                                = RenderPrefabAssetData;
        static constexpr std::string_view ASSET_TYPE  = "RenderPrefab";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using RenderPrefabAssetPtr = std::shared_ptr<Asset<RenderPrefab>>;
}