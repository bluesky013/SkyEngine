#include <aurora/adaptor/assets/MeshAsset.h>
#include <aurora/rhi/VertexSemantic.h>

#include <cmath>
#include <unordered_set>

namespace sky::aurora {
    namespace {
        bool FiniteBounds(const AABB &bounds)
        {
            for (uint32_t axis = 0; axis < 3; ++axis) {
                if (!std::isfinite(bounds.min[axis]) || !std::isfinite(bounds.max[axis]) || bounds.min[axis] > bounds.max[axis]) {
                    return false;
                }
            }
            return true;
        }
        bool ReadName(BinaryInputArchive &archive, std::string &name)
        {
            uint32_t size = 0;
            archive.LoadValue(size);
            if (!archive.IsValid() || size == 0 || size > 4096) {
                return false;
            }
            name.resize(size);
            archive.LoadValue(name.data(), size);
            return archive.IsValid() && name.find('\0') == std::string::npos;
        }
    } // namespace
    SkinStatus MeshAssetData::Validate() const
    {
        if (version != CURRENT_VERSION) {
            return {SkinError::UnsupportedVersion, "Mesh v5 or older requires recook for independent Skin products"};
        }
        if (!vertexCount || vertexCount > 16U * 1024U * 1024U || !vertexStride || vertexStride > 256 ||
            vertexSize != uint64_t(vertexCount) * vertexStride || vertexOffset > UINT64_MAX - vertexSize || !indexCount || indexCount % 3 ||
            (indexType != uint32_t(IndexType::U16) && indexType != uint32_t(IndexType::U32)) ||
            indexSize != uint64_t(indexCount) * (indexType == uint32_t(IndexType::U16) ? 2 : 4) || indexOffset > UINT64_MAX - indexSize ||
            attributes.empty() || attributes.size() > 16 || subMeshes.empty() || subMeshes.size() > 8192 || materials.size() > 8192 ||
            !FiniteBounds(bounds.ToAABB()) || !std::isfinite(bounds.radius) || bounds.radius < 0) {
            return {SkinError::InvalidData, "Mesh header ranges or bounds are invalid"};
        }
        bool                         joints = false, weights = false, position = false;
        std::unordered_set<uint16_t> semantics;
        for (const auto &attribute : attributes) {
            if (attribute.offset >= vertexStride || !semantics.insert(uint16_t(attribute.semantic) * 256 + attribute.semanticIndex).second) {
                return {SkinError::InvalidData, "Mesh attributes are duplicated or exceed the vertex stride"};
            }
            const auto semantic = static_cast<VertexSemantic>(attribute.semantic);
            position |= semantic == VertexSemantic::POSITION;
            joints |= semantic == VertexSemantic::JOINTS;
            weights |= semantic == VertexSemantic::WEIGHTS;
            if ((semantic == VertexSemantic::JOINTS && (attribute.format != uint32_t(Format::U_RGBA8) || vertexStride - attribute.offset < 4)) ||
                (semantic == VertexSemantic::WEIGHTS && (attribute.format != uint32_t(Format::F_RGBA32) || vertexStride - attribute.offset < 16))) {
                return {SkinError::InvalidData, "Skin streams require integer U_RGBA8 slots and F_RGBA32 weights"};
            }
        }
        if (!position || joints != weights || joints != HasSkinInputs() || skinInput.slots.size() > 256 ||
            slotBounds.size() != skinInput.slots.size() || (joints && !skinInput.modelSpaceSignature)) {
            return {SkinError::InvalidData, "Mesh Skin input layout is inconsistent"};
        }
        std::unordered_set<std::string> slots;
        for (size_t i = 0; i < skinInput.slots.size(); ++i) {
            const auto &slot = skinInput.slots[i];
            if (slot.empty() || slot.size() > 4096 || slot.find('\0') != std::string::npos || !slots.insert(slot).second ||
                (slotBounds[i].occupied && !FiniteBounds(slotBounds[i].bounds))) {
                return {SkinError::InvalidData, "Mesh has invalid Skin slot identities or bounds"};
            }
        }
        uint64_t end = 0;
        for (const auto &sub : subMeshes) {
            if (sub.indexOffset != end || !sub.indexCount || sub.indexCount % 3 || sub.indexCount > indexCount - end ||
                (!materials.empty() && sub.materialIndex >= materials.size())) {
                return {SkinError::InvalidData, "Mesh submesh ranges are invalid"};
            }
            end += sub.indexCount;
        }
        if (end != indexCount) {
            return {SkinError::InvalidData, "Mesh submeshes do not cover the index stream"};
        }
        return {};
    }
    void MeshAssetData::Save(BinaryOutputArchive &ar) const
    {
        ar.SaveValue(version);
        ar.SaveValue(vertexOffset);
        ar.SaveValue(vertexSize);
        ar.SaveValue(vertexStride);
        ar.SaveValue(vertexCount);
        ar.SaveValue(uint32_t(attributes.size()));
        for (const auto &value : attributes) {
            ar.SaveValue(value.semantic);
            ar.SaveValue(value.semanticIndex);
            ar.SaveValue(value.format);
            ar.SaveValue(value.offset);
        }
        ar.SaveValue(indexOffset);
        ar.SaveValue(indexSize);
        ar.SaveValue(indexCount);
        ar.SaveValue(indexType);
        ar.SaveValue(skinInput.modelSpaceSignature);
        ar.SaveValue(uint32_t(skinInput.slots.size()));
        for (size_t slot = 0; slot < skinInput.slots.size(); ++slot) {
            ar.SaveValue(skinInput.slots[slot]);
            const auto box = slot < slotBounds.size() ? slotBounds[slot] : MeshSkinSlotBounds{};
            ar.SaveValue(uint8_t(box.occupied));
            ar.SaveValue(box.bounds.min);
            ar.SaveValue(box.bounds.max);
        }
        ar.SaveValue(uint32_t(subMeshes.size()));
        for (const auto &value : subMeshes) {
            ar.SaveValue(value.indexOffset);
            ar.SaveValue(value.indexCount);
            ar.SaveValue(value.materialIndex);
        }
        ar.SaveValue(uint32_t(materials.size()));
        for (const auto &value : materials) {
            ar.SaveValue(value);
        }
        ar.SaveValue(bounds.center);
        ar.SaveValue(bounds.extent);
        ar.SaveValue(bounds.radius);
    }
    void MeshAssetData::Load(BinaryInputArchive &ar)
    {
        clear();
        validation = {SkinError::InvalidData, "truncated or invalid Mesh v6 header"};
        ar.LoadValue(version);
        if (!ar.IsValid() || version != CURRENT_VERSION) {
            validation = {SkinError::UnsupportedVersion, "Mesh product requires recook: expected v6 with independent Skin assets"};
            LOG_E("AuroraMeshAsset", "%s", validation.message.c_str());
            return;
        }
        ar.LoadValue(vertexOffset);
        ar.LoadValue(vertexSize);
        ar.LoadValue(vertexStride);
        ar.LoadValue(vertexCount);
        uint32_t count = 0;
        ar.LoadValue(count);
        if (!ar.IsValid() || count > 16) {
            return;
        }
        attributes.resize(count);
        for (auto &value : attributes) {
            ar.LoadValue(value.semantic);
            ar.LoadValue(value.semanticIndex);
            ar.LoadValue(value.format);
            ar.LoadValue(value.offset);
        }
        ar.LoadValue(indexOffset);
        ar.LoadValue(indexSize);
        ar.LoadValue(indexCount);
        ar.LoadValue(indexType);
        ar.LoadValue(skinInput.modelSpaceSignature);
        ar.LoadValue(count);
        if (!ar.IsValid() || count > 256) {
            return;
        }
        skinInput.slots.resize(count);
        slotBounds.resize(count);
        for (size_t slot = 0; slot < count; ++slot) {
            if (!ReadName(ar, skinInput.slots[slot])) {
                return;
            }
            uint8_t occupied = 0;
            ar.LoadValue(occupied);
            if (occupied > 1) {
                return;
            }
            slotBounds[slot].occupied = occupied != 0;
            ar.LoadValue(slotBounds[slot].bounds.min);
            ar.LoadValue(slotBounds[slot].bounds.max);
        }
        ar.LoadValue(count);
        if (!ar.IsValid() || count > 8192) {
            return;
        }
        subMeshes.resize(count);
        for (auto &value : subMeshes) {
            ar.LoadValue(value.indexOffset);
            ar.LoadValue(value.indexCount);
            ar.LoadValue(value.materialIndex);
        }
        ar.LoadValue(count);
        if (!ar.IsValid() || count > 8192) {
            return;
        }
        materials.resize(count);
        for (auto &value : materials) {
            ar.LoadValue(value);
        }
        ar.LoadValue(bounds.center);
        ar.LoadValue(bounds.extent);
        ar.LoadValue(bounds.radius);
        if (ar.IsValid()) {
            validation = Validate();
        }
    }
    void MeshAssetData::clear()
    {
        *this = MeshAssetData{};
    }
} // namespace sky::aurora
