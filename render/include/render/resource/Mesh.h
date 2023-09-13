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
        RDMaterialPtr material;
        AABB aabb;
    };

    class Mesh {
    public:
        Mesh() = default;
        ~Mesh() = default;

        void AddSubMesh(const SubMesh &sub);
        void AddVertexBuffer(const RDBufferPtr &vb);
        void SetIndexBuffer(const RDBufferPtr &ib);

    private:
        std::vector<SubMesh> subMeshes;
        std::vector<RDBufferPtr> vertexBuffers;
        RDBufferPtr indexBuffer;
    };

    using RDMeshPtr = std::shared_ptr<Mesh>;

} // namespace sky