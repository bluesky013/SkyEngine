//
// MeshletBuilder (see MeshletBuilder.h).
//

#include <aurora/cook/mesh/MeshletBuilder.h>

#include <aurora/rhi/Core.h>
#include <core/logger/Logger.h>

#include <meshoptimizer.h>

#include <cstring>

static const char *TAG = "AuroraMeshCook";

namespace sky::aurora::cook {

    void MeshletBuilder::DoWork()
    {
        auto &mesh = *payload.mesh;
        if (mesh.vertexCount == 0 || mesh.indexCount == 0) {
            return;
        }

        // meshopt works on u32 indices
        const size_t            indexCount = mesh.indexCount;
        std::vector<uint32_t>   indices(indexCount);
        if (mesh.indexType == static_cast<uint32_t>(IndexType::U16)) {
            const auto *src = reinterpret_cast<const uint16_t *>(mesh.indexData.data());
            for (size_t i = 0; i < indexCount; ++i) {
                indices[i] = src[i];
            }
        } else {
            std::memcpy(indices.data(), mesh.indexData.data(), indexCount * sizeof(uint32_t));
        }

        const size_t maxMeshlets =
            meshopt_buildMeshletsBound(indexCount, payload.maxVertices, payload.maxTriangles);
        std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
        mesh.meshletVertices.resize(maxMeshlets * payload.maxVertices);
        mesh.meshletTriangles.resize(maxMeshlets * payload.maxTriangles * 3);

        const float *positions = reinterpret_cast<const float *>(mesh.vertexData.data()); // POSITION at offset 0
        const size_t meshletCount =
            meshopt_buildMeshlets(meshlets.data(), mesh.meshletVertices.data(), mesh.meshletTriangles.data(),
                                  indices.data(), indexCount, positions, mesh.vertexCount, mesh.vertexStride,
                                  payload.maxVertices, payload.maxTriangles, payload.coneWeight);

        mesh.meshlets.resize(meshletCount);
        mesh.meshletBounds.resize(meshletCount);

        uint32_t vertexUsed   = 0;
        uint32_t triangleUsed = 0;
        for (size_t i = 0; i < meshletCount; ++i) {
            const auto &src = meshlets[i];

            MeshletData dst;
            dst.vertexOffset   = src.vertex_offset;
            dst.triangleOffset = src.triangle_offset;
            dst.vertexCount    = src.vertex_count;
            dst.triangleCount  = src.triangle_count;
            mesh.meshlets[i] = dst;

            const meshopt_Bounds bounds =
                meshopt_computeMeshletBounds(&mesh.meshletVertices[src.vertex_offset],
                                             &mesh.meshletTriangles[src.triangle_offset], src.triangle_count,
                                             positions, mesh.vertexCount, mesh.vertexStride);
            auto &out         = mesh.meshletBounds[i];
            std::memcpy(out.center, bounds.center, sizeof(out.center));
            out.radius = bounds.radius;
            std::memcpy(out.coneApex, bounds.cone_apex, sizeof(out.coneApex));
            std::memcpy(out.coneAxis, bounds.cone_axis, sizeof(out.coneAxis));
            out.coneCutoff = bounds.cone_cutoff;
            std::memcpy(out.coneAxisS8, bounds.cone_axis_s8, sizeof(out.coneAxisS8));
            out.coneCutoffS8 = bounds.cone_cutoff_s8;

            vertexUsed   = dst.vertexOffset + dst.vertexCount > vertexUsed ? dst.vertexOffset + dst.vertexCount : vertexUsed;
            triangleUsed = dst.triangleOffset + dst.triangleCount * 3 > triangleUsed
                               ? dst.triangleOffset + dst.triangleCount * 3
                               : triangleUsed;
        }
        mesh.meshletVertices.resize(vertexUsed);
        mesh.meshletTriangles.resize(triangleUsed);

        LOG_I(TAG, "built %zu meshlets (%u vertices, %u triangle indices)", meshletCount, vertexUsed, triangleUsed);
    }

} // namespace sky::aurora::cook
