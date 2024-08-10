//
// Created by Zach Lee on 2023/2/26.
//

#pragma once

#include <string>
#include <vector>

#include <core/shapes/AABB.h>
#include <core/archive/StreamArchive.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetCommon.h>

#include <rhi/Core.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/BufferAsset.h>
#include <render/resource/Mesh.h>
#include <animation/skeleton/Skeleton.h>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    enum class MeshType : uint32_t  {
        STANDARD = 0,
        SKINNED
    };

    struct SubMeshAssetData {
        uint32_t firstVertex = 0;
        uint32_t vertexCount = 0;
        uint32_t firstIndex  = 0;
        uint32_t indexCount  = 0;
        Uuid material;
        AABB aabb;
    };

    struct MeshPrimitiveHeader {
        uint32_t offset;
        uint32_t size;
        uint32_t stride;
    };

    struct MeshIndicesHeader {
        uint32_t offset;
        uint32_t size;
        rhi::IndexType indexType = rhi::IndexType::U32;
    };

    struct MeshDataHeader {
        uint32_t version = 0;
        MeshType meshType = MeshType::STANDARD;
        Uuid skeleton;
        std::vector<SubMeshAssetData> subMeshes;
        std::vector<std::string> vertexDescriptions;
        std::vector<MeshPrimitiveHeader> primitives;
        MeshIndicesHeader indices;
        uint32_t dataSize;
    };

    struct MeshAssetData : MeshDataHeader {
        uint32_t dataOffset;
        AssetRawData rawData;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    template <>
    struct AssetTraits<Mesh> {
        using DataType                                = MeshAssetData;
        static constexpr std::string_view ASSET_TYPE  = "Mesh";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using MeshAssetPtr = std::shared_ptr<Asset<Mesh>>;

    CounterPtr<Mesh> CreateMeshFromAsset(const MeshAssetPtr &asset);
}