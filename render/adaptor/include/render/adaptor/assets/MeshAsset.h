//
// Created by Zach Lee on 2023/2/26.
//

#pragma once

#include <string>
#include <vector>

#include <framework/asset/AssetManager.h>
#include <core/shapes/AABB.h>

#include <rhi/Core.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/BufferAsset.h>
#include <render/resource/Mesh.h>


namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct SubMeshAssetData {
        uint32_t firstVertex = 0;
        uint32_t vertexCount = 0;
        uint32_t firstIndex  = 0;
        uint32_t indexCount  = 0;
        MaterialAssetPtr material;
        AABB aabb;
    };

    struct MeshAssetData {
        std::vector<SubMeshAssetData> subMeshes;
        std::vector<BufferAssetPtr> vertexBuffers;
        BufferAssetPtr indexBuffer;
        std::vector<std::string> vertexDescriptions;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    std::shared_ptr<Mesh> CreateMesh(const MeshAssetData &data);

    template <>
    struct AssetTraits<Mesh> {
        using DataType                                = MeshAssetData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("394AB7FF-FC10-484F-82A6-42D523949DD1");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;

        static std::shared_ptr<Mesh> CreateFromData(const DataType &data)
        {
            return CreateMesh(data);
        }
    };
    using MeshAssetPtr = std::shared_ptr<Asset<Mesh>>;
}