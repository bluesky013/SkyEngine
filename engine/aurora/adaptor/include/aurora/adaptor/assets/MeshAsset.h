//
// Aurora mesh asset: CPU payload (vertex/index bytes + sub-mesh ranges + bounds)
// that the bridge turns into an aurora::Mesh when a device is available.
//

#pragma once

#include <aurora/resource/Mesh.h>
#include <core/logger/Logger.h>
#include <core/shapes/AABB.h>
#include <core/util/Uuid.h>
#include <framework/asset/Asset.h>
#include <framework/serialization/BinaryArchive.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sky::aurora {

    struct MeshSubMeshData {
        uint32_t indexOffset   = 0;
        uint32_t indexCount    = 0;
        uint32_t materialIndex = 0; // index into MeshAssetData::materials
    };

    struct MeshAssetData {
        static constexpr uint32_t CURRENT_VERSION = 1;

        uint32_t                     version = CURRENT_VERSION;
        std::vector<uint8_t>         vertexData;
        std::vector<uint8_t>         indexData;
        std::vector<MeshSubMeshData> subMeshes;
        std::vector<Uuid>            materials; // material slot table indexed by MeshSubMeshData::materialIndex
        AABB                         bounds;

        // Returns null when there is no slot table; an out-of-range index falls
        // back to slot 0. Only slot 0 is a valid fallback because sub-meshes with
        // materialIndex == 0 are the common single-material case.
        const Uuid *GetMaterialUuid(uint32_t materialIndex) const
        {
            if (materials.empty()) {
                return nullptr;
            }
            if (materialIndex >= materials.size()) {
                LOG_W("AuroraMeshAsset", "material index %u out of range (slots: %u); falling back to slot 0", materialIndex,
                      static_cast<uint32_t>(materials.size()));
                return &materials[0];
            }
            return &materials[materialIndex];
        }

        void Save(BinaryOutputArchive &ar) const
        {
            ar.SaveValue(version);
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
            ar.SaveValue(static_cast<uint32_t>(materials.size()));
            for (const auto &material : materials) {
                ar.SaveValue(material.ToString());
            }
            ar.SaveValue(bounds.min);
            ar.SaveValue(bounds.max);
        }

        void Load(BinaryInputArchive &ar)
        {
            ar.LoadValue(version);
            if (version != CURRENT_VERSION) {
                LOG_E("AuroraMeshAsset", "unsupported mesh asset version: %u (expected %u)", version, CURRENT_VERSION);
                clear();
                return;
            }

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
            uint32_t materialCount = 0;
            ar.LoadValue(materialCount);
            materials.resize(materialCount);
            for (auto &material : materials) {
                std::string materialStr;
                ar.LoadValue(materialStr);
                material = Uuid::CreateFromString(materialStr);
            }
            ar.LoadValue(bounds.min);
            ar.LoadValue(bounds.max);
        }

        void clear()
        {
            vertexData.clear();
            indexData.clear();
            subMeshes.clear();
            materials.clear();
            bounds = AABB{};
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
