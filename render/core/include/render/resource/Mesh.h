//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

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

    class Mesh {
    public:
        Mesh() = default;
        ~Mesh() = default;

        void AddSubMesh(const SubMesh &sub);
        void AddVertexBuffer(const RDBufferPtr &vb);
        void SetIndexBuffer(const RDBufferPtr &ib);
        void SetIndexType(rhi::IndexType type);
        void AddVertexDescriptions(const std::string &key);

        const std::vector<SubMesh> &GetSubMeshes() const { return subMeshes; }
        const std::vector<RDBufferPtr> &GetVertexBuffers() const { return vertexBuffers; }
        const std::vector<std::string> &GetVertexDescriptions() const { return vertexDescriptions; }
        const RDBufferPtr &GetIndexBuffer() const { return indexBuffer; }
        rhi::IndexType GetIndexType() const { return indexType; }

    private:
        std::vector<SubMesh> subMeshes;
        std::vector<RDBufferPtr> vertexBuffers;
        std::vector<std::string> vertexDescriptions;
        RDBufferPtr indexBuffer;
        rhi::IndexType indexType = rhi::IndexType::U32;
    };

    using RDMeshPtr = std::shared_ptr<Mesh>;

} // namespace sky