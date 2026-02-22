//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <core/shapes/AABB.h>
#include <render/RenderGeometry.h>
#include <render/RenderResource.h>
#include <render/resource/Material.h>
#include <render/resource/MeshInterface.h>

namespace sky {

    struct MeshSubSection {
        uint32_t firstVertex = 0;
        uint32_t vertexCount = 0;
        uint32_t firstIndex  = 0;
        uint32_t indexCount  = 0;
        uint32_t firstMeshlet = 0;
        uint32_t meshletCount = 0;
        uint32_t materialIndex = 0;
        AABB aabb;
    };

    // TODO
    struct SubMesh {
        uint32_t firstVertex = 0;
        uint32_t vertexCount = 0;
        uint32_t firstIndex  = 0;
        uint32_t indexCount  = 0;
        uint32_t firstMeshlet = 0;
        uint32_t meshletCount = 0;
        RDMaterialInstancePtr material;
        AABB aabb;
    };

    struct VertexBufferSource {
        rhi::BufferUploadRequest source;
        uint32_t                 stride;
    };

    struct MeshUploadData {
        std::vector<VertexBufferSource> vertexStreams;
        rhi::BufferUploadRequest        indexStream;
        rhi::BufferUploadRequest        meshlets;
        rhi::BufferUploadRequest        meshletVertices;
        rhi::BufferUploadRequest        meshletTriangles;
    };

    class Mesh : public RenderResource {
    public:
        Mesh();
        ~Mesh() override = default;

        // builder
        void AddSubMesh(const SubMesh &sub);
        void SetVertexAttributes(const std::vector<VertexAttribute> &attributes);
        void SetUploadStream(MeshUploadData&& stream);
        void SetIndexType(rhi::IndexType type);
        void SetMaterial(const RDMaterialInstancePtr &mat, uint32_t subMesh);

        void SetBoundingBox(const AABB &aabb);

        // geometry
        const RDMaterialInstancePtr &GetMaterialBySubMesh(uint32_t section) const { return subMeshes[section].material; }
        const std::vector<SubMesh> &GetSubMeshes() const { return subMeshes; }
        const RenderGeometryPtr& GetGeometry() const { return geometry; }

        // type
        virtual bool HasSkin() const { return false; }

        bool ClusterValid() const { return clusterValid; }
    protected:
        // desc
        std::vector<SubMesh> subMeshes;
        RenderGeometryPtr    geometry;

        MeshUploadData meshData;

        bool clusterValid = false;
    };

    using RDMeshPtr = CounterPtr<Mesh>;

} // namespace sky