//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <render/RenderResource.h>
#include <render/resource/Buffer.h>
#include <render/resource/Material.h>
#include <core/shapes/AABB.h>
#include <memory>

namespace sky {

    struct SubMesh {
        uint32_t firstVertex = 0;
        uint32_t vertexCount = 0;
        uint32_t firstIndex  = 0;
        uint32_t indexCount  = 0;
        RDMaterialInstancePtr material;
        AABB aabb;
    };

    struct MeshData {
        std::vector<rhi::BufferUploadRequest> vertexStreams;
        rhi::BufferUploadRequest indexStream;
    };

    class Mesh : public RenderResource {
    public:
        Mesh() = default;
        ~Mesh() override = default;

        // builder
        void AddSubMesh(const SubMesh &sub);
        void AddVertexDescriptions(const std::string &key);
        void SetIndexType(rhi::IndexType type);

        // upload
        void SetUploadStream(MeshData&& stream);
        void UploadMeshData();

        void AddVertexBuffer(const RDBufferPtr &vb);
        void SetIndexBuffer(const RDBufferPtr &ib);
        void SetMaterial(const RDMaterialInstancePtr &mat, uint32_t subMesh);

        const std::vector<SubMesh> &GetSubMeshes() const { return subMeshes; }
        const std::vector<RDBufferPtr> &GetVertexBuffers() const { return vertexBuffers; }
        const std::vector<std::string> &GetVertexDescriptions() const { return vertexDescriptions; }
        const RDBufferPtr &GetIndexBuffer() const { return indexBuffer; }
        rhi::IndexType GetIndexType() const { return indexType; }
    private:
        // desc
        std::vector<SubMesh> subMeshes;
        std::vector<std::string> vertexDescriptions;
        rhi::IndexType indexType = rhi::IndexType::U32;

        // render resource
        std::vector<RDBufferPtr> vertexBuffers;
        RDBufferPtr indexBuffer;

        // upload handle
        MeshData streamData;
        rhi::TransferTaskHandle uploadHandle {};
    };

    using RDMeshPtr = CounterPtr<Mesh>;

} // namespace sky