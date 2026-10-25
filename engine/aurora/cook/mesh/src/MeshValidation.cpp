#include <animation/core/Skeleton.h>
#include <aurora/cook/mesh/MeshProcess.h>
#include <aurora/rhi/VertexSemantic.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace sky::aurora::cook {
    namespace {
        MeshCookStatus Invalid(std::string message)
        {
            return {MeshCookError::InvalidData, std::move(message)};
        }
        template <class T>
        bool Finite(const T &value, size_t count)
        {
            for (size_t i = 0; i < count; ++i) {
                if (!std::isfinite(value[i])) {
                    return false;
                }
            }
            return true;
        }
        bool FiniteMatrix(const Matrix4 &value)
        {
            for (const float element : value.v) {
                if (!std::isfinite(element)) {
                    return false;
                }
            }
            return value[0][3] == 0.f && value[1][3] == 0.f && value[2][3] == 0.f && value[3][3] == 1.f && std::isfinite(value.Determinant()) &&
                   value.Determinant() != 0.f;
        }
        void Include(AABB &box, const Vector3 &point, bool first)
        {
            for (size_t axis = 0; axis < 3; ++axis) {
                box.min[axis] = first ? point[axis] : std::min(box.min[axis], point[axis]);
                box.max[axis] = first ? point[axis] : std::max(box.max[axis], point[axis]);
            }
        }
        bool Contains(const AABB &box, const Vector3 &point)
        {
            for (size_t axis = 0; axis < 3; ++axis) {
                if (!std::isfinite(box.min[axis]) || !std::isfinite(box.max[axis]) || point[axis] < box.min[axis] - 1e-5f ||
                    point[axis] > box.max[axis] + 1e-5f) {
                    return false;
                }
            }
            return true;
        }
        const MeshVertexAttributeData *Attribute(const CookedMesh &mesh, VertexSemantic semantic)
        {
            const auto found =
                std::find_if(mesh.attributes.begin(), mesh.attributes.end(), [semantic](const auto &a) { return a.semantic == uint8_t(semantic); });
            return found == mesh.attributes.end() ? nullptr : &*found;
        }
        uint32_t Size(const MeshVertexAttributeData &attribute)
        {
            switch (static_cast<Format>(attribute.format)) {
            case Format::F_RGB32: return 12;
            case Format::F_RGBA32: return 16;
            case Format::F_RG32: return 8;
            case Format::U_RGBA8: return 4;
            default: return 0;
            }
        }
        MeshCookStatus StreamRanges(const CookedMesh &mesh)
        {
            if (!mesh.vertexCount || !mesh.indexCount || mesh.indexCount % 3 || mesh.vertexStride > 256 || mesh.attributes.empty() ||
                mesh.vertexData.size() != uint64_t(mesh.vertexCount) * mesh.vertexStride ||
                (mesh.indexType != uint32_t(IndexType::U16) && mesh.indexType != uint32_t(IndexType::U32)) ||
                mesh.indexData.size() != uint64_t(mesh.indexCount) * (mesh.indexType == uint32_t(IndexType::U16) ? 2 : 4)) {
                return Invalid("cooked vertex or index stream has invalid ranges");
            }
            for (const auto &a : mesh.attributes) {
                if (!Size(a) || a.offset >= mesh.vertexStride || Size(a) > mesh.vertexStride - a.offset) {
                    return Invalid("cooked attribute exceeds stride");
                }
            }
            const auto *position = Attribute(mesh, VertexSemantic::POSITION);
            if (!position || position->format != uint32_t(Format::F_RGB32)) {
                return Invalid("cooked positions require F_RGB32");
            }
            const auto *joints  = Attribute(mesh, VertexSemantic::JOINTS);
            const auto *weights = Attribute(mesh, VertexSemantic::WEIGHTS);
            const bool  skinned = !mesh.skinInput.slots.empty();
            if (bool(joints) != skinned || bool(weights) != skinned || mesh.skinInput.slots.size() > 256 ||
                (skinned && (!mesh.skinInput.modelSpaceSignature || joints->format != uint32_t(Format::U_RGBA8) ||
                             weights->format != uint32_t(Format::F_RGBA32)))) {
                return Invalid("cooked Skin input contract does not match vertex attributes");
            }
            uint64_t end = 0;
            for (const auto &sub : mesh.subMeshes) {
                if (sub.indexOffset != end || !sub.indexCount || sub.indexCount % 3 || sub.indexCount > mesh.indexCount - end) {
                    return Invalid("invalid submesh index ranges");
                }
                end += sub.indexCount;
            }
            if (end != mesh.indexCount) {
                return Invalid("submesh ranges do not cover all indices");
            }
            for (size_t index = 0; index < mesh.indexCount; ++index) {
                uint32_t   value = 0;
                const auto bytes = mesh.indexType == uint32_t(IndexType::U16) ? 2 : 4;
                std::memcpy(&value, mesh.indexData.data() + index * bytes, bytes);
                if (value >= mesh.vertexCount) {
                    return Invalid("cooked index is out of range");
                }
            }
            return {};
        }
        MeshCookStatus VisitVertices(const CookedMesh &mesh, AABB &bounds, std::vector<MeshSkinSlotBounds> &slots)
        {
            if (const auto status = StreamRanges(mesh); !status) {
                return status;
            }
            const auto *position = Attribute(mesh, VertexSemantic::POSITION);
            const auto *joints   = Attribute(mesh, VertexSemantic::JOINTS);
            const auto *weights  = Attribute(mesh, VertexSemantic::WEIGHTS);
            slots.assign(mesh.skinInput.slots.size(), {});
            for (uint32_t vertex = 0; vertex < mesh.vertexCount; ++vertex) {
                const auto *data = mesh.vertexData.data() + size_t(vertex) * mesh.vertexStride;
                Vector3     point;
                std::memcpy(&point, data + position->offset, sizeof(point));
                if (!Finite(point, 3)) {
                    return Invalid("cooked position is nonfinite");
                }
                Include(bounds, point, vertex == 0);
                for (const auto &attribute : mesh.attributes) {
                    if (attribute.format == uint32_t(Format::U_RGBA8)) {
                        continue;
                    }
                    float value[4]{};
                    std::memcpy(value, data + attribute.offset, Size(attribute));
                    if (!Finite(value, Size(attribute) / 4)) {
                        return Invalid("cooked vertex attribute is nonfinite");
                    }
                }
                if (!joints) {
                    continue;
                }
                Vector4 values;
                std::memcpy(&values, data + weights->offset, sizeof(values));
                double sum = 0;
                for (uint32_t influence = 0; influence < 4; ++influence) {
                    const uint32_t slot   = data[joints->offset + influence];
                    const float    weight = values[influence];
                    if (slot >= slots.size() || weight < 0 || !std::isfinite(weight) || (weight == 0 && slot != 0)) {
                        return Invalid("invalid cooked Skin influence");
                    }
                    sum += weight;
                    if (weight > 0) {
                        Include(slots[slot].bounds, point, !slots[slot].occupied);
                        slots[slot].occupied = true;
                    }
                }
                if (std::abs(sum - 1.0) > 1e-5) {
                    return Invalid("cooked Skin weights are not normalized");
                }
            }
            return {};
        }
    } // namespace
    MeshCookStatus ValidateMeshSource(const CookMeshSource &source)
    {
        if (!source.Valid()) {
            return Invalid("mesh has no primitives");
        }
        if (source.Skinned()) {
            if (const auto status = Skin::Validate(source.skin); !status) {
                return Invalid(status.message);
            }
            SkeletonData skeleton;
            for (const auto &bone : source.bones) {
                skeleton.boneData.push_back({Name(bone.name.c_str()), bone.parentIndex});
                skeleton.refPos.push_back(bone.refTransform);
            }
            if (const auto status = Skeleton::Validate(skeleton); !status) {
                return Invalid(status.message);
            }
        }
        uint64_t vertices = 0, indices = 0;
        for (const auto &primitive : source.primitives) {
            const auto count = primitive.positions.size();
            if (!count || count > 16U * 1024U * 1024U || primitive.indices.empty() || primitive.indices.size() % 3 ||
                !FiniteMatrix(primitive.transform) || (!primitive.normals.empty() && primitive.normals.size() != count) ||
                (!primitive.tangents.empty() && primitive.tangents.size() != count) || (!primitive.uvs.empty() && primitive.uvs.size() != count) ||
                (!primitive.colors.empty() && primitive.colors.size() != count) ||
                (source.Skinned() && (primitive.joints.size() != count || primitive.weights.size() != count)) ||
                (!source.Skinned() && (!primitive.joints.empty() || !primitive.weights.empty()))) {
                return Invalid("source primitive streams or transform are invalid");
            }
            vertices += count;
            indices += primitive.indices.size();
            if (vertices > 16U * 1024U * 1024U || indices > UINT32_MAX) {
                return Invalid("mesh exceeds supported stream sizes");
            }
            for (const auto index : primitive.indices) {
                if (index >= count) {
                    return Invalid("source vertex index is out of range");
                }
            }
            for (size_t vertex = 0; vertex < count; ++vertex) {
                if (!Finite(primitive.positions[vertex], 3) ||
                    (!primitive.normals.empty() && (!Finite(primitive.normals[vertex], 3) || primitive.normals[vertex].Length() == 0)) ||
                    (!primitive.tangents.empty() && !Finite(primitive.tangents[vertex], 4)) ||
                    (!primitive.uvs.empty() && !Finite(primitive.uvs[vertex], 2)) ||
                    (!primitive.colors.empty() && !Finite(primitive.colors[vertex], 4))) {
                    return Invalid("source vertex contains a nonfinite or invalid direction");
                }
                if (!source.Skinned()) {
                    continue;
                }
                double sum = 0;
                for (size_t influence = 0; influence < 4; ++influence) {
                    const float slot   = primitive.joints[vertex][influence];
                    const float weight = primitive.weights[vertex][influence];
                    if (!std::isfinite(slot) || !std::isfinite(weight) || slot < 0 || std::floor(slot) != slot || slot >= source.skin.binds.size() ||
                        weight < 0 || (weight == 0 && slot != 0)) {
                        return Invalid("source Skin slot or weight is invalid");
                    }
                    sum += weight;
                }
                if (std::abs(sum - 1) > 1e-5) {
                    return Invalid("source vertex weights are not normalized");
                }
            }
        }
        return {};
    }
    MeshCookStatus RefreshMeshBounds(CookedMesh &mesh)
    {
        AABB                            bounds;
        std::vector<MeshSkinSlotBounds> slots;
        if (const auto status = VisitVertices(mesh, bounds, slots); !status) {
            return status;
        }
        mesh.bounds     = bounds;
        mesh.slotBounds = std::move(slots);
        return {};
    }
    MeshCookStatus ValidateCookedMesh(const CookedMesh &mesh)
    {
        AABB                            actual;
        std::vector<MeshSkinSlotBounds> slots;
        if (const auto status = VisitVertices(mesh, actual, slots); !status) {
            return status;
        }
        if (!Contains(mesh.bounds, actual.min) || !Contains(mesh.bounds, actual.max) || slots.size() != mesh.slotBounds.size()) {
            return Invalid("cooked bounds do not contain the geometry");
        }
        for (size_t i = 0; i < slots.size(); ++i) {
            if (slots[i].occupied != mesh.slotBounds[i].occupied ||
                (slots[i].occupied &&
                 (!Contains(mesh.slotBounds[i].bounds, slots[i].bounds.min) || !Contains(mesh.slotBounds[i].bounds, slots[i].bounds.max)))) {
                return Invalid("Skin slot bounds do not match the weighted vertices");
            }
        }
        return {};
    }
} // namespace sky::aurora::cook
