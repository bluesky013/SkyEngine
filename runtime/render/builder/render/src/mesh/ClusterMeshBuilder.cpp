//
// Created by Zach Lee on 2024/12/20.
//

#include <builder/render/mesh/ClusterMeshBuilder.h>
#include <render/resource/Meshlet.h>
#include <meshoptimizer.h>
#include "MetisInclude.h"

namespace sky::builder {

    static constexpr size_t MAX_VERTICES  = 64;
    static constexpr size_t MAX_TRIANGLES = 124;
    static constexpr float  CONE_WEIGHTS  = 0.0f;

    MeshletBound FromOptBound(const meshopt_Bounds &bound)
    {
        MeshletBound result = {};
        result.center   = Vector4(bound.center[0], bound.center[1], bound.center[2], bound.radius);
        result.coneApex = Vector4(bound.cone_apex[0], bound.cone_apex[1], bound.cone_apex[2], 0.f);
        result.coneAxis = Vector4(bound.cone_axis[0], bound.cone_axis[1], bound.cone_axis[2], bound.cone_cutoff);
        return result;
    }

    static void BuildCluster(const uint32_t* indices, uint32_t indexCount, const float* vertexPos, size_t vertexCount, uint32_t stride,
        MeshletData& outData)
    {
        size_t maxMeshlets = meshopt_buildMeshletsBound(indexCount, MAX_VERTICES, MAX_TRIANGLES);

        SKY_ASSERT(sizeof(Meshlet) == sizeof(meshopt_Meshlet));
        outData.meshlets.resize(maxMeshlets);
        outData.meshletVertices.resize(maxMeshlets * MAX_VERTICES);
        outData.meshletTriangles.resize(maxMeshlets * MAX_TRIANGLES * 3);

        size_t meshletCount = meshopt_buildMeshlets(
            reinterpret_cast<meshopt_Meshlet*>(outData.meshlets.data()),
            outData.meshletVertices.data(),
            outData.meshletTriangles.data(),
            indices, indexCount,
            vertexPos, vertexCount, stride,
            MAX_VERTICES, MAX_TRIANGLES, CONE_WEIGHTS);

//        meshopt_optimizeMeshlet(outData.meshletVertices.data(), outData.meshletTriangles.data(), MAX_TRIANGLES, MAX_VERTICES);
        outData.meshlets.resize(meshletCount);
        outData.meshletBounds.reserve(meshletCount);
        for (auto &meshlet : outData.meshlets) {
            meshopt_Bounds bounds = meshopt_computeMeshletBounds(
                &outData.meshletVertices[meshlet.vertexOffset], &outData.meshletTriangles[meshlet.triangleOffset],
                meshlet.triangleCount, vertexPos, vertexCount, sizeof(stride));
            outData.meshletBounds.emplace_back(FromOptBound(bounds));
        }
    }

    ClusterMeshBuilder::ClusterMeshBuilder()
    {
        int options[METIS_NOPTIONS];
        METIS_SetDefaultOptions(options);
        options[METIS_OPTION_SEED] = 42;
    }

    void ClusterMeshBuilder::BuildFromMeshData(const MeshAssetData &data)
    {
        auto iter = std::find_if(data.attributes.begin(), data.attributes.end(),
            [](const VertexAttribute &val) -> bool {
            return val.sematic.TestBit(VertexSemanticFlagBit::POSITION);
        });
        // no position in mesh data!
        if (iter == data.attributes.end()) {
            return;
        }

        SKY_ASSERT(iter->binding < data.buffers.size());
        SKY_ASSERT(data.indexBuffer < data.buffers.size());
        const auto &posView = data.buffers[iter->binding];
        const auto &idxView = data.buffers[data.indexBuffer];

        const auto *position = reinterpret_cast<const float*>(data.rawData.storage.data() + posView.offset);
        SKY_ASSERT(data.indexType == rhi::IndexType::U32);
        const auto *indices = reinterpret_cast<const uint32_t*>(data.rawData.storage.data() + idxView.offset);

        for (const auto &subMesh : data.subMeshes) {
            MeshletData meshletData = {};

            const float* subPos = position + subMesh.firstVertex;
            const uint32_t *subIdx = indices + subMesh.firstIndex;

            BuildCluster(subIdx, subMesh.indexCount, subPos, subMesh.vertexCount, posView.stride, meshletData);
        }
    }

} // namespace sky::builder
