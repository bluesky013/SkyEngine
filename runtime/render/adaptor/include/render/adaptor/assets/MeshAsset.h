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

    struct MeshPrimitiveHeader {
        uint32_t size;
        uint32_t stride;
    };

    struct MeshIndicesHeader {
        uint32_t size;
        rhi::IndexType indexType = rhi::IndexType::U32;
    };

    struct MeshAssetData {
        std::vector<SubMeshAssetData> subMeshes;
        std::vector<std::string> vertexDescriptions;
        std::vector<MeshPrimitiveHeader> primitives;
        MeshIndicesHeader indices;

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