// Mesh geometry and its ordered Skin input contract; binding assets remain independent.

#pragma once

#include <aurora/resource/Mesh.h>
#include <aurora/resource/Skin.h>
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

    using MeshSkinSlotBounds = SkinSlotBounds;

    struct MeshAssetData {
        static constexpr uint32_t CURRENT_VERSION = 6;

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

        SkinInputLayout                 skinInput;
        std::vector<MeshSkinSlotBounds> slotBounds;

        std::vector<MeshSubMeshData> subMeshes;
        std::vector<Uuid>            materials; // material slot table indexed by MeshSubMeshData::materialIndex
        BoundingBoxSphere            bounds;

        // Build-side scratch (not serialized); moved into the product bulk via
        // AssetBase::AddBulk before Save.
        std::vector<uint8_t> vertexData;
        std::vector<uint8_t> indexData;

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

        bool HasSkinInputs() const
        {
            return !skinInput.slots.empty();
        }
        void Save(BinaryOutputArchive &ar) const;
        void Load(BinaryInputArchive &ar);
        bool IsLoadValid() const
        {
            return static_cast<bool>(validation);
        }
        SkinStatus Validate() const;
        void       clear();
        SkinStatus validation;
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
