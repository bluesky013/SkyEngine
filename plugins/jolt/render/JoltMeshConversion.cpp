#include "JoltMeshConversion.h"

#include <algorithm>
#include <aurora/rhi/VertexSemantic.h>
#include <cmath>
#include <cstring>

namespace sky::phy {
    CounterPtr<TriangleMesh> ConvertAuroraCollisionMesh(const Asset<aurora::Mesh> &asset, std::string &error)
    {
        error.clear();
        const auto fail = [&error](const char *reason) -> CounterPtr<TriangleMesh> {
            error = reason;
            return {};
        };
        const auto        &data      = asset.Data();
        const auto         indexType = static_cast<aurora::IndexType>(data.indexType);
        const uint32_t     indexSize = indexType == aurora::IndexType::U16 ? 2 : 4;
        constexpr uint64_t maxBytes  = 256u * 1024u * 1024u;
        if (data.version != aurora::MeshAssetData::CURRENT_VERSION || data.vertexCount == 0 || data.vertexStride < sizeof(Vector3) ||
            data.indexCount == 0 || data.indexCount % 3 != 0 || data.vertexSize > maxBytes || data.indexSize > maxBytes ||
            data.vertexSize != uint64_t(data.vertexCount) * data.vertexStride || data.indexSize != uint64_t(data.indexCount) * indexSize ||
            (indexType != aurora::IndexType::U16 && indexType != aurora::IndexType::U32)) {
            return fail("Collision mesh has invalid vertex/index ranges");
        }
        const aurora::MeshVertexAttributeData *position = nullptr;
        for (const auto &attribute : data.attributes) {
            if (attribute.semantic == uint8_t(aurora::VertexSemantic::POSITION) && attribute.semanticIndex == 0) {
                if (position || attribute.format != uint32_t(aurora::Format::F_RGB32) || attribute.offset > data.vertexStride - sizeof(Vector3)) {
                    return fail("Collision mesh requires one float3 POSITION within the vertex stride");
                }
                position = &attribute;
            }
        }
        if (!position)
            return fail("Collision mesh has no POSITION attribute");
        const auto vertices = data.vertexData.empty() ? asset.ReadBulk(data.vertexOffset, data.vertexSize) : data.vertexData;
        auto       indices  = data.indexData.empty() ? asset.ReadBulk(data.indexOffset, data.indexSize) : data.indexData;
        if (vertices.size() != data.vertexSize || indices.size() != data.indexSize)
            return fail("Collision mesh bulk read is incomplete");
        CounterPtr<TriangleMesh> mesh = new TriangleMesh();
        mesh->vtxStride               = sizeof(Vector3);
        mesh->position.resize(size_t(data.vertexCount) * sizeof(Vector3));
        for (uint32_t index = 0; index < data.vertexCount; ++index) {
            Vector3 point;
            std::memcpy(&point, vertices.data() + size_t(index) * data.vertexStride + position->offset, sizeof(point));
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
                return fail("Collision mesh position is not finite");
            std::memcpy(mesh->position.data() + size_t(index) * sizeof(point), &point, sizeof(point));
        }
        for (uint32_t index = 0; index < data.indexCount; ++index) {
            uint32_t vertex = 0;
            if (indexSize == 2) {
                uint16_t value;
                std::memcpy(&value, indices.data() + size_t(index) * indexSize, sizeof(value));
                vertex = value;
            } else {
                std::memcpy(&vertex, indices.data() + size_t(index) * indexSize, sizeof(vertex));
            }
            if (vertex >= data.vertexCount)
                return fail("Collision mesh index is out of range");
        }
        mesh->indexType = indexSize == 2 ? IndexType::U16 : IndexType::U32;
        mesh->indexRaw  = std::move(indices);
        for (const auto &sub : data.subMeshes) {
            if (sub.indexCount == 0 || sub.indexCount % 3 != 0 || sub.indexOffset > data.indexCount ||
                sub.indexCount > data.indexCount - sub.indexOffset) {
                return fail("Collision mesh submesh range is invalid");
            }
            mesh->AddView(0, data.vertexCount, sub.indexOffset, sub.indexCount, data.bounds.ToAABB());
        }
        if (data.subMeshes.empty())
            mesh->AddView(0, data.vertexCount, 0, data.indexCount, data.bounds.ToAABB());
        return mesh;
    }
} // namespace sky::phy
