//
// Aurora mesh asset: header (vertex/index/skin section table + sub-meshes +
// bounds + refs) plus a bulk holding the vertex, index and skin bytes. The
// bridge turns it into an aurora::Mesh when a device is available; skin data is
// absorbed into the mesh bulk (no standalone AuroraSkin asset).
//

#pragma once

#include <aurora/resource/Mesh.h>
#include <core/logger/Logger.h>
#include <core/math/Matrix4.h>
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

    struct MeshAssetData {
        // v5: header + bulk (vertex/index/skin sections); skin absorbed from its
        // own asset; v4: dropped meshlets; v3: skeleton ref; v2: BoundingBoxSphere
        // + vertex stream description + skin
        static constexpr uint32_t CURRENT_VERSION = 5;

        uint32_t version = CURRENT_VERSION;

        // ---- vertex section (bulk) ----
        uint64_t                             vertexOffset = 0;
        uint64_t                             vertexSize   = 0;
        uint32_t                             vertexStride = 0;
        uint32_t                             vertexCount  = 0;
        std::vector<MeshVertexAttributeData> attributes;

        // ---- index section (bulk) ----
        uint64_t indexOffset = 0;
        uint64_t indexSize   = 0;
        uint32_t indexCount  = 0;
        uint32_t indexType   = 0; // aurora::IndexType

        // ---- skin section (bulk) ----
        // Layout: inverseBindMatrices (skinBoneCount * sizeof(Matrix4)) followed by
        // boneMapping (skinMappingCount * sizeof(uint32_t)). boneNames live in the
        // header (index-aligned with inverseBindMatrices).
        uint64_t                 skinOffset       = 0;
        uint64_t                 skinSize         = 0;
        uint32_t                 skinBoneCount    = 0;
        uint32_t                 skinMappingCount = 0;
        std::vector<std::string> skinBoneNames;

        std::vector<MeshSubMeshData> subMeshes;
        std::vector<Uuid>            materials; // material slot table indexed by MeshSubMeshData::materialIndex
        BoundingBoxSphere            bounds;

        // optional skeleton (bone hierarchy) in its own AuroraSkeleton asset
        Uuid skeleton; // invalid -> no skeleton

        // Build-side scratch (not serialized); moved into the product bulk via
        // AssetBase::AddBulk before Save.
        std::vector<uint8_t> vertexData;
        std::vector<uint8_t> indexData;
        std::vector<uint8_t> skinData;

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

        bool HasSkin() const
        {
            return skinSize != 0;
        }
        bool HasSkeleton() const
        {
            return static_cast<bool>(skeleton);
        }

        void Save(BinaryOutputArchive &ar) const
        {
            ar.SaveValue(version);
            ar.SaveValue(vertexOffset);
            ar.SaveValue(vertexSize);
            ar.SaveValue(vertexStride);
            ar.SaveValue(vertexCount);
            ar.SaveValue(static_cast<uint32_t>(attributes.size()));
            for (const auto &attr : attributes) {
                ar.SaveValue(attr.semantic);
                ar.SaveValue(attr.semanticIndex);
                ar.SaveValue(attr.format);
                ar.SaveValue(attr.offset);
            }
            ar.SaveValue(indexOffset);
            ar.SaveValue(indexSize);
            ar.SaveValue(indexCount);
            ar.SaveValue(indexType);
            ar.SaveValue(skinOffset);
            ar.SaveValue(skinSize);
            ar.SaveValue(skinBoneCount);
            ar.SaveValue(skinMappingCount);
            ar.SaveValue(static_cast<uint32_t>(skinBoneNames.size()));
            for (const auto &name : skinBoneNames) {
                ar.SaveValue(name);
            }
            ar.SaveValue(skeleton);
            ar.SaveValue(static_cast<uint32_t>(subMeshes.size()));
            for (const auto &sub : subMeshes) {
                ar.SaveValue(sub.indexOffset);
                ar.SaveValue(sub.indexCount);
                ar.SaveValue(sub.materialIndex);
            }
            ar.SaveValue(static_cast<uint32_t>(materials.size()));
            for (const auto &material : materials) {
                ar.SaveValue(material);
            }
            ar.SaveValue(bounds.center);
            ar.SaveValue(bounds.extent);
            ar.SaveValue(bounds.radius);
        }

        void Load(BinaryInputArchive &ar)
        {
            ar.LoadValue(version);
            if (version != CURRENT_VERSION) {
                LOG_E("AuroraMeshAsset", "unsupported mesh asset version: %u (expected %u)", version, CURRENT_VERSION);
                clear();
                return;
            }

            ar.LoadValue(vertexOffset);
            ar.LoadValue(vertexSize);
            ar.LoadValue(vertexStride);
            ar.LoadValue(vertexCount);
            uint32_t count = 0;
            ar.LoadValue(count);
            attributes.resize(count);
            for (auto &attr : attributes) {
                ar.LoadValue(attr.semantic);
                ar.LoadValue(attr.semanticIndex);
                ar.LoadValue(attr.format);
                ar.LoadValue(attr.offset);
            }
            ar.LoadValue(indexOffset);
            ar.LoadValue(indexSize);
            ar.LoadValue(indexCount);
            ar.LoadValue(indexType);
            ar.LoadValue(skinOffset);
            ar.LoadValue(skinSize);
            ar.LoadValue(skinBoneCount);
            ar.LoadValue(skinMappingCount);
            ar.LoadValue(count);
            skinBoneNames.resize(count);
            for (auto &name : skinBoneNames) {
                ar.LoadValue(name);
            }
            ar.LoadValue(skeleton);
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
                ar.LoadValue(material);
            }
            ar.LoadValue(bounds.center);
            ar.LoadValue(bounds.extent);
            ar.LoadValue(bounds.radius);
        }

        void clear()
        {
            vertexOffset = 0;
            vertexSize   = 0;
            vertexStride = 0;
            vertexCount  = 0;
            attributes.clear();
            indexOffset      = 0;
            indexSize        = 0;
            indexCount       = 0;
            indexType        = 0;
            skinOffset       = 0;
            skinSize         = 0;
            skinBoneCount    = 0;
            skinMappingCount = 0;
            skinBoneNames.clear();
            subMeshes.clear();
            materials.clear();
            bounds   = BoundingBoxSphere{};
            skeleton = Uuid{};
            vertexData.clear();
            indexData.clear();
            skinData.clear();
        }
    };

    // Runtime-only: build the mesh-side Skin from the absorbed skin section of the
    // mesh bulk (inverse bind matrices + bone mapping). Returns null when the mesh
    // is not skinned. boneMatrices start empty (the animation bridge fills them).
    CounterPtr<Skin> CreateSkinFromMeshAsset(const Asset<Mesh> &asset);

} // namespace sky::aurora

namespace sky {

    template <>
    struct AssetTraits<sky::aurora::Mesh> {
        using DataType                                   = sky::aurora::MeshAssetData;
        static constexpr std::string_view ASSET_TYPE     = "AuroraMesh";
        static constexpr SerializeType    SERIALIZE_TYPE = SerializeType::BIN;
    };

} // namespace sky
