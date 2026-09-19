//
// Aurora mesh asset: CPU payload (vertex/index bytes + sub-mesh ranges + bounds)
// that the bridge turns into an aurora::Mesh when a device is available.
//

#pragma once

#include <aurora/resource/Mesh.h>
#include <core/logger/Logger.h>
#include <core/shapes/Bounds.h>
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

    // one entry of the interleaved vertex stream; semantics/format map to
    // aurora::VertexSemantic / aurora::Format (stored as plain integers so the
    // asset format does not depend on rhi header churn)
    struct MeshVertexAttributeData {
        uint8_t  semantic      = 0;
        uint8_t  semanticIndex = 0;
        uint16_t reserved      = 0;
        uint32_t format        = 0; // aurora::Format
        uint32_t offset        = 0;
    };

    // meshopt meshlet record, mirrored so the asset format does not depend on
    // meshoptimizer headers
    struct MeshletData {
        uint32_t vertexOffset   = 0;
        uint32_t triangleOffset = 0;
        uint32_t vertexCount    = 0;
        uint32_t triangleCount  = 0;
    };

    struct MeshletBoundsData {
        float   center[3]     = {};
        float   radius        = 0.f;
        float   coneApex[3]   = {};
        float   coneAxis[3]   = {};
        float   coneCutoff    = 0.f;
        int8_t  coneAxisS8[3] = {};
        int8_t  coneCutoffS8  = 0;
    };

    struct MeshAssetData {
        // v2: bounds AABB -> BoundingBoxSphere, vertex stream description, skin, meshlets
        static constexpr uint32_t CURRENT_VERSION = 2;

        uint32_t                     version = CURRENT_VERSION;
        std::vector<uint8_t>         vertexData; // single interleaved stream
        std::vector<uint8_t>         indexData;
        std::vector<MeshSubMeshData> subMeshes;
        std::vector<Uuid>            materials; // material slot table indexed by MeshSubMeshData::materialIndex
        BoundingBoxSphere            bounds;

        // v2: vertex stream description (required to rebuild VertexLayout)
        uint32_t                            vertexStride = 0;
        uint32_t                            vertexCount  = 0;
        uint32_t                            indexCount   = 0;
        uint32_t                            indexType    = 0; // aurora::IndexType
        std::vector<MeshVertexAttributeData> attributes;

        // v2: optional skinning, skin payload lives in its own AuroraSkin asset
        Uuid skin; // invalid -> not skinned

        // v2: optional meshlet payload (empty = not built)
        std::vector<MeshletData>       meshlets;
        std::vector<uint32_t>          meshletVertices;
        std::vector<uint8_t>           meshletTriangles;
        std::vector<MeshletBoundsData> meshletBounds;

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

        bool HasMeshlets() const { return !meshlets.empty(); }
        bool HasSkin() const { return static_cast<bool>(skin); }

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
            ar.SaveValue(vertexStride);
            ar.SaveValue(vertexCount);
            ar.SaveValue(indexCount);
            ar.SaveValue(indexType);
            ar.SaveValue(static_cast<uint32_t>(attributes.size()));
            for (const auto &attr : attributes) {
                ar.SaveValue(attr.semantic);
                ar.SaveValue(attr.semanticIndex);
                ar.SaveValue(attr.format);
                ar.SaveValue(attr.offset);
            }
            ar.SaveValue(skin.ToString());
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
            ar.SaveValue(bounds.center);
            ar.SaveValue(bounds.extent);
            ar.SaveValue(bounds.radius);

            ar.SaveValue(static_cast<uint32_t>(meshlets.size()));
            if (!meshlets.empty()) {
                ar.SaveValue(reinterpret_cast<const char *>(meshlets.data()), meshlets.size() * sizeof(MeshletData));
                ar.SaveValue(static_cast<uint32_t>(meshletVertices.size()));
                ar.SaveValue(reinterpret_cast<const char *>(meshletVertices.data()), meshletVertices.size() * sizeof(uint32_t));
                ar.SaveValue(static_cast<uint32_t>(meshletTriangles.size()));
                ar.SaveValue(reinterpret_cast<const char *>(meshletTriangles.data()), meshletTriangles.size());
                ar.SaveValue(reinterpret_cast<const char *>(meshletBounds.data()), meshletBounds.size() * sizeof(MeshletBoundsData));
            }
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
            ar.LoadValue(vertexStride);
            ar.LoadValue(vertexCount);
            ar.LoadValue(indexCount);
            ar.LoadValue(indexType);
            uint32_t count = 0;
            ar.LoadValue(count);
            attributes.resize(count);
            for (auto &attr : attributes) {
                ar.LoadValue(attr.semantic);
                ar.LoadValue(attr.semanticIndex);
                ar.LoadValue(attr.format);
                ar.LoadValue(attr.offset);
            }
            std::string skinStr;
            ar.LoadValue(skinStr);
            skin = Uuid::CreateFromString(skinStr);
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
            ar.LoadValue(bounds.center);
            ar.LoadValue(bounds.extent);
            ar.LoadValue(bounds.radius);

            ar.LoadValue(count);
            meshlets.resize(count);
            if (count != 0) {
                ar.LoadValue(reinterpret_cast<char *>(meshlets.data()), count * sizeof(MeshletData));
                uint32_t meshletVertexCount = 0;
                ar.LoadValue(meshletVertexCount);
                meshletVertices.resize(meshletVertexCount);
                ar.LoadValue(reinterpret_cast<char *>(meshletVertices.data()), meshletVertexCount * sizeof(uint32_t));
                uint32_t meshletTriangleCount = 0;
                ar.LoadValue(meshletTriangleCount);
                meshletTriangles.resize(meshletTriangleCount);
                ar.LoadValue(reinterpret_cast<char *>(meshletTriangles.data()), meshletTriangleCount);
                meshletBounds.resize(count);
                ar.LoadValue(reinterpret_cast<char *>(meshletBounds.data()), count * sizeof(MeshletBoundsData));
            }
        }

        void clear()
        {
            vertexData.clear();
            indexData.clear();
            subMeshes.clear();
            materials.clear();
            bounds = BoundingBoxSphere{};
            vertexStride = 0;
            vertexCount  = 0;
            indexCount   = 0;
            indexType    = 0;
            attributes.clear();
            skin = Uuid{};
            meshlets.clear();
            meshletVertices.clear();
            meshletTriangles.clear();
            meshletBounds.clear();
        }
    };

} // namespace sky::aurora

namespace sky {

    template <>
    struct AssetTraits<sky::aurora::Mesh> {
        using DataType                                   = sky::aurora::MeshAssetData;
        static constexpr std::string_view ASSET_TYPE     = "AuroraMesh";
        static constexpr SerializeType    SERIALIZE_TYPE = SerializeType::BIN;
    };

} // namespace sky
