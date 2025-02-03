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
        MeshMeshletsData& outData)
    {
        size_t maxMeshlets = meshopt_buildMeshletsBound(indexCount, MAX_VERTICES, MAX_TRIANGLES);

        std::vector<meshopt_Meshlet> meshlets;
        meshlets.resize(maxMeshlets);
        outData.meshletVertices.resize(maxMeshlets * MAX_VERTICES);
        outData.meshletTriangles.resize(maxMeshlets * MAX_TRIANGLES * 3);

        size_t meshletCount = meshopt_buildMeshlets(
            meshlets.data(),
            outData.meshletVertices.data(),
            outData.meshletTriangles.data(),
            indices, indexCount,
            vertexPos, vertexCount, stride,
            MAX_VERTICES, MAX_TRIANGLES, CONE_WEIGHTS);

        outData.meshlets.resize(meshletCount);
        for (uint32_t i = 0; i < meshletCount; ++i) {
            const auto &meshlet = meshlets[i];
            auto &dstMeshlet = outData.meshlets[i];

            uint32_t *meshletVertices = &outData.meshletVertices[meshlet.vertex_offset];
            uint8_t *meshletTriangles = &outData.meshletTriangles[meshlet.triangle_offset];

            meshopt_optimizeMeshlet(meshletVertices, meshletTriangles,
                meshlet.triangle_count, meshlet.vertex_count);

            meshopt_Bounds bounds = meshopt_computeMeshletBounds(meshletVertices, meshletTriangles,
                meshlet.vertex_count, vertexPos, vertexCount, stride);

            dstMeshlet.vertexOffset   = meshlet.vertex_offset;
            dstMeshlet.triangleOffset = meshlet.triangle_offset;
            dstMeshlet.vertexCount    = meshlet.vertex_count;
            dstMeshlet.triangleCount  = meshlet.triangle_count;
            dstMeshlet.bounds = FromOptBound(bounds);
        }
    }

    static void MergeMeshletData(MeshAssetData &meshData, const std::vector<MeshMeshletsData>& meshletDatas, MeshMeshletsData &merged)
    {
        auto subMeshSize = meshData.subMeshes.size();

        for (uint32_t i = 0; i < subMeshSize; ++i) {
            auto &subMesh = meshData.subMeshes[i];
            const auto &subMeshlets = meshletDatas[i];

            subMesh.firstMeshlet = static_cast<uint32_t>(merged.meshlets.size());
            subMesh.meshletCount = static_cast<uint32_t>(subMeshlets.meshlets.size());

            auto meshletVerticesOffset = static_cast<uint32_t>(merged.meshletVertices.size());
            auto meshletTriangleOffset = static_cast<uint32_t>(merged.meshletTriangles.size());

            merged.meshlets.resize(subMesh.firstMeshlet + subMesh.meshletCount);
            for (uint32_t j = 0; j < subMesh.meshletCount; ++j) {
                const auto &src = subMeshlets.meshlets[j];
                auto &dst = merged.meshlets[subMesh.firstMeshlet + j];
                dst = src;
                dst.vertexOffset += meshletVerticesOffset;
                dst.triangleOffset += meshletTriangleOffset;
            }

            {
                merged.meshletVertices.resize(meshletVerticesOffset + subMeshlets.meshletVertices.size());
                const uint32_t *src = subMeshlets.meshletVertices.data();
                uint32_t *dst = &merged.meshletVertices[meshletVerticesOffset];

                memcpy(dst, src, subMeshlets.meshletVertices.size() * sizeof(uint32_t));
            }

            {
                merged.meshletTriangles.resize(meshletTriangleOffset + subMeshlets.meshletTriangles.size());
                const uint8_t *src = subMeshlets.meshletTriangles.data();
                uint8_t *dst = &merged.meshletTriangles[meshletTriangleOffset];

                memcpy(dst, src, subMeshlets.meshletTriangles.size() * sizeof(uint8_t));
            }
        }
    }

    static void FillMeshAssetData(MeshAssetData &meshData, const MeshMeshletsData &merged)
    {
        auto meshletsSize = static_cast<uint32_t>(merged.meshlets.size() * sizeof(Meshlet));
        auto meshletVerticesSize = static_cast<uint32_t>(merged.meshletVertices.size() * sizeof(uint32_t));
        auto meshletTrianglesSize = static_cast<uint32_t>(merged.meshletTriangles.size() * sizeof(uint8_t));

        meshData.dataSize += (meshletsSize + meshletVerticesSize + meshletTrianglesSize);
        auto baseOffset = static_cast<uint32_t>(meshData.rawData.storage.size());
        meshData.rawData.storage.resize(meshData.dataSize);

        // save meshlets
        {
            meshData.meshlets = static_cast<uint32_t>(meshData.buffers.size());
            meshData.buffers.emplace_back(MeshBufferView{
                baseOffset, meshletVerticesSize, sizeof(uint32_t), MeshBufferType::RAW_DATA
            });
            memcpy(meshData.rawData.storage.data() + baseOffset, reinterpret_cast<const char*>(merged.meshlets.data()), meshletsSize);

            baseOffset += meshletsSize;
        }

        // save vertices
        {
            meshData.meshletVertices = static_cast<uint32_t>(meshData.buffers.size());
            meshData.buffers.emplace_back(MeshBufferView{
                baseOffset, meshletsSize, sizeof(uint32_t), MeshBufferType::RAW_DATA
            });
            memcpy(meshData.rawData.storage.data() + baseOffset, reinterpret_cast<const char*>(merged.meshletVertices.data()), meshletVerticesSize);

            baseOffset += meshletVerticesSize;
        }


        // save triangles
        {
            meshData.meshletTriangles = static_cast<uint32_t>(meshData.buffers.size());
            meshData.buffers.emplace_back(MeshBufferView{
                baseOffset, meshletsSize, sizeof(uint8_t), MeshBufferType::RAW_DATA
            });
            memcpy(meshData.rawData.storage.data() + baseOffset, reinterpret_cast<const char*>(merged.meshletTriangles.data()), meshletTrianglesSize);

            baseOffset += meshletTrianglesSize;
        }

    }

    ClusterMeshBuilder::ClusterMeshBuilder() = default;
//    {
//        int options[METIS_NOPTIONS];
//        METIS_SetDefaultOptions(options);
//        options[METIS_OPTION_SEED] = 42;
//    }

    void ClusterMeshBuilder::BuildFromMeshData(MeshAssetData &data)
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

        std::vector<MeshMeshletsData> meshletDatas(data.subMeshes.size());
        for (uint32_t i = 0; i < data.subMeshes.size(); ++i) {
            const auto &subMesh = data.subMeshes[i];
            auto &meshletData = meshletDatas[i];

            const float* subPos = position + subMesh.firstVertex * posView.stride;
            const uint32_t *subIdx = indices + subMesh.firstIndex;
            BuildCluster(subIdx, subMesh.indexCount, subPos, subMesh.vertexCount, posView.stride, meshletData);
        }

        // merge meshlet data
        MeshMeshletsData merged;
        MergeMeshletData(data, meshletDatas, merged);
        FillMeshAssetData(data, merged);
    }

} // namespace sky::builder
