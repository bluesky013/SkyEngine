//
// Created by Zach Lee on 2023/2/26.
//

#pragma once

#include <string>
#include <vector>

#include <core/shapes/AABB.h>
#include <core/shapes/TriangleMesh.h>
#include <core/archive/StreamArchive.h>

#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetCommon.h>
#include <framework/interface/IMeshConfigNotify.h>

#include <rhi/Core.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/BufferAsset.h>
#include <render/resource/Mesh.h>
#include <render/resource/Meshlet.h>
#include <animation/skeleton/Skeleton.h>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    enum class MeshBufferType : uint32_t {
        RAW_DATA = 0,
        UUID
    };

    struct MeshBufferView {
        uint32_t offset;
        uint32_t size;
        uint32_t stride;
        MeshBufferType type = MeshBufferType::RAW_DATA;
    };

    struct SubMeshAssetData {
        uint32_t firstVertex = 0;
        uint32_t vertexCount = 0;
        uint32_t firstIndex  = 0;
        uint32_t indexCount  = 0;
        uint32_t firstMeshlet = 0;
        uint32_t meshletCount = 0;
        Uuid material;
        AABB aabb;
    };

    template <typename T>
    struct TBufferViewAccessor {
        const uint8_t *ptr = nullptr;
        uint32_t offset = 0;
        uint32_t stride = 0;
        uint32_t count = 0;

        const T& operator[](uint32_t index) const
        {
            SKY_ASSERT(index < count);
            return *reinterpret_cast<const T*>(ptr + index * stride + offset);
        }
    };

    static constexpr uint32_t INVALID_MESH_BUFFER_VIEW = ~(0U);

    struct MeshDataHeader {
        uint32_t version  = 0;
        Uuid     skeleton;

        std::vector<SubMeshAssetData> subMeshes;
        std::vector<MeshBufferView>   buffers;
        std::vector<VertexAttribute>  attributes;
        uint32_t       indexBuffer = INVALID_MESH_BUFFER_VIEW;
        rhi::IndexType indexType = rhi::IndexType::U32;
        uint32_t meshlets = INVALID_MESH_BUFFER_VIEW;
        uint32_t meshletVertices = INVALID_MESH_BUFFER_VIEW;
        uint32_t meshletTriangles = INVALID_MESH_BUFFER_VIEW;

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
    CounterPtr<TriangleMesh> CreateTriangleMesh(const MeshAssetPtr &asset);
}