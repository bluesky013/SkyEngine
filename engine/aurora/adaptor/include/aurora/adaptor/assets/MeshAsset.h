//
// Aurora mesh asset: CPU payload (vertex/index bytes + sub-mesh ranges + bounds)
// that the bridge turns into an aurora::Mesh when a device is available.
//

#pragma once

#include <aurora/resource/Mesh.h>
#include <core/shapes/AABB.h>
#include <framework/asset/Asset.h>
#include <framework/serialization/BinaryArchive.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace sky::aurora {

    struct MeshSubMeshData {
        uint32_t indexOffset   = 0;
        uint32_t indexCount    = 0;
        uint32_t materialIndex = 0;
    };

    struct MeshAssetData {
        std::vector<uint8_t>         vertexData;
        std::vector<uint8_t>         indexData;
        std::vector<MeshSubMeshData> subMeshes;
        AABB                         bounds;

        void Save(BinaryOutputArchive &ar) const
        {
            ar.SaveValue(static_cast<uint32_t>(vertexData.size()));
            if (!vertexData.empty()) {
                ar.SaveValue(reinterpret_cast<const char *>(vertexData.data()), vertexData.size());
            }
            ar.SaveValue(static_cast<uint32_t>(indexData.size()));
            if (!indexData.empty()) {
                ar.SaveValue(reinterpret_cast<const char *>(indexData.data()), indexData.size());
            }
            ar.SaveValue(static_cast<uint32_t>(subMeshes.size()));
            for (const auto &sub : subMeshes) {
                ar.SaveValue(sub.indexOffset);
                ar.SaveValue(sub.indexCount);
                ar.SaveValue(sub.materialIndex);
            }
            ar.SaveValue(bounds.min);
            ar.SaveValue(bounds.max);
        }

        void Load(BinaryInputArchive &ar)
        {
            uint32_t size = 0;
            ar.LoadValue(size);
            vertexData.resize(size);
            if (size != 0) {
                ar.LoadValue(reinterpret_cast<char *>(vertexData.data()), size);
            }
            ar.LoadValue(size);
            indexData.resize(size);
            if (size != 0) {
                ar.LoadValue(reinterpret_cast<char *>(indexData.data()), size);
            }
            uint32_t count = 0;
            ar.LoadValue(count);
            subMeshes.resize(count);
            for (auto &sub : subMeshes) {
                ar.LoadValue(sub.indexOffset);
                ar.LoadValue(sub.indexCount);
                ar.LoadValue(sub.materialIndex);
            }
            ar.LoadValue(bounds.min);
            ar.LoadValue(bounds.max);
        }
    };

} // namespace sky::aurora

namespace sky {

    template <>
    struct AssetTraits<sky::aurora::Mesh> {
        using DataType                                = sky::aurora::MeshAssetData;
        static constexpr std::string_view ASSET_TYPE  = "AuroraMesh";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };

} // namespace sky
