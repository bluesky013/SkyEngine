//
// MeshOptimizer (see MeshOptimizer.h).
//

#include <aurora/cook/mesh/MeshOptimizer.h>

#include <aurora/rhi/Core.h>
#include <core/logger/Logger.h>

#include <meshoptimizer.h>

#include <cstring>

static const char *TAG = "AuroraMeshCook";

namespace sky::aurora::cook {

    MeshCookStatus MeshOptimizer::DoWork()
    {
        if (!payload.mesh) {
            return {MeshCookError::InvalidData, "missing mesh optimizer input"};
        }
        auto &mesh = *payload.mesh;
        if (const auto status = ValidateCookedMesh(mesh); !status) {
            return status;
        }
        const uint32_t srcVertexCount = mesh.vertexCount;

        const bool            srcU16     = mesh.indexType == static_cast<uint32_t>(IndexType::U16);
        const size_t          indexCount = mesh.indexCount;
        std::vector<uint32_t> indices(indexCount);
        if (srcU16) {
            const auto *src = reinterpret_cast<const uint16_t *>(mesh.indexData.data());
            for (size_t i = 0; i < indexCount; ++i) {
                indices[i] = src[i];
            }
        } else {
            std::memcpy(indices.data(), mesh.indexData.data(), indexCount * sizeof(uint32_t));
        }

        std::vector<uint32_t> optimized(indexCount);
        const float          *positions = reinterpret_cast<const float *>(mesh.vertexData.data());
        for (const auto &sub : mesh.subMeshes) {
            auto *destination = optimized.data() + sub.indexOffset;
            meshopt_optimizeVertexCache(destination, indices.data() + sub.indexOffset, sub.indexCount, mesh.vertexCount);
            meshopt_optimizeOverdraw(destination, destination, sub.indexCount, positions, mesh.vertexCount, mesh.vertexStride, 1.05f);
        }

        std::vector<uint8_t> vertices(mesh.vertexData.size());
        const size_t         vertexCount =
            meshopt_optimizeVertexFetch(vertices.data(), optimized.data(), indexCount, mesh.vertexData.data(), mesh.vertexCount, mesh.vertexStride);

        const bool dstU16 = vertexCount < 65536;
        mesh.vertexData   = std::move(vertices);
        mesh.vertexData.resize(vertexCount * mesh.vertexStride);
        mesh.vertexCount = static_cast<uint32_t>(vertexCount);
        mesh.indexType   = dstU16 ? static_cast<uint32_t>(IndexType::U16) : static_cast<uint32_t>(IndexType::U32);
        mesh.indexData.resize(indexCount * (dstU16 ? 2 : 4));
        if (dstU16) {
            auto *dst = reinterpret_cast<uint16_t *>(mesh.indexData.data());
            for (size_t i = 0; i < indexCount; ++i) {
                dst[i] = static_cast<uint16_t>(optimized[i]);
            }
        } else {
            std::memcpy(mesh.indexData.data(), optimized.data(), indexCount * sizeof(uint32_t));
        }

        LOG_I(TAG, "optimized mesh: %u -> %u vertices", srcVertexCount, mesh.vertexCount);
        if (const auto status = RefreshMeshBounds(mesh); !status) {
            return status;
        }
        return ValidateCookedMesh(mesh);
    }

} // namespace sky::aurora::cook
