//
// Created by Zach Lee on 2023/2/26.
//

#pragma once

#include <core/math/Box.h>
#include <framework/asset/AssetManager.h>
#include <render/adaptor/assets/Material.h>
#include <rhi/Core.h>
#include <string>
#include <vector>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct VertexDesc {
        std::string name;
        uint32_t    index  = 0;
        uint32_t    offset = 0;
        rhi::Format format = rhi::Format::UNDEFINED;
    };

    struct SubMeshAssetData {
        uint32_t firstVertex = 0;
        uint32_t vertexCount = 0;
        uint32_t firstIndex  = 0;
        uint32_t indexCount  = 0;
        MaterialAssetPtr material;
        Box aabb;
    };

    struct MeshAssetData {
        std::vector<VertexDesc> vertexDescriptions;
        std::vector<SubMeshAssetData> subMeshes;
        std::vector<std::vector<uint8_t>> vertexBuffers;
        std::vector<uint8_t> indexBuffer;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    class Mesh {
        Mesh() = default;
        ~Mesh() = default;
    };

    template <>
    struct AssetTraits<Mesh> {
        using DataType                                = MeshAssetData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("394AB7FF-FC10-484F-82A6-42D523949DD1");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using MeshAssetPtr = std::shared_ptr<Asset<Mesh>>;
}