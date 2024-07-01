//
// Created by Zach Lee on 2023/2/26.
//

#pragma once

#include <string>
#include <vector>

#include <core/shapes/AABB.h>
#include <framework/asset/AssetManager.h>

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
        Uuid material;
        AABB aabb;
    };

    struct BufferViewData {
        Uuid buffer;
        uint32_t offset;
        uint32_t size;
    };

    struct MeshAssetData {
        std::vector<SubMeshAssetData> subMeshes;
        std::vector<BufferViewData> vertexBuffers;
        BufferViewData indexBuffer;
        rhi::IndexType indexType = rhi::IndexType::U32;
        std::vector<std::string> vertexDescriptions;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    std::shared_ptr<Mesh> CreateMesh(const MeshAssetData &data);

    template <>
    struct AssetTraits<Mesh> {
        using DataType                                = MeshAssetData;
        static constexpr std::string_view ASSET_TYPE  = "Mesh";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;

        static std::shared_ptr<Mesh> CreateFromData(const DataType &data)
        {
            return CreateMesh(data);
        }
    };
    using MeshAssetPtr = std::shared_ptr<Asset<Mesh>>;
}