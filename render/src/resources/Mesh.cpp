//
// Created by Zach Lee on 2022/5/26.
//

#include <render/resources/Mesh.h>

namespace sky {

    Mesh::Builder::Builder(Mesh& m) : mesh(m)
    {
    }

    Mesh::Builder& Mesh::Builder::SetIndexBuffer(const RDBufferViewPtr& buffer, VkIndexType type)
    {
        mesh.indexBuffer = std::move(buffer);
        mesh.indexType = type;
        return *this;
    }

    Mesh::Builder& Mesh::Builder::AddVertexBuffer(const RDBufferViewPtr& buffer)
    {
        mesh.vertexBuffers.emplace_back(buffer);
        return *this;
    }

    Mesh::Builder& Mesh::Builder::AddVertexDesc(const VertexDesc& desc)
    {
        mesh.vertexDescriptions.emplace_back(desc);
        return *this;
    }

    Mesh::Builder& Mesh::Builder::AddSubMesh(const SubMesh& subMesh)
    {
        mesh.subMeshes.emplace_back(subMesh);
        return *this;
    }

    const std::vector<SubMesh>& Mesh::GetSubMeshes() const
    {
        return subMeshes;
    }

    const std::vector<VertexDesc>& Mesh::GetVertexDesc() const
    {
        return vertexDescriptions;
    }

    const std::vector<RDBufferViewPtr>& Mesh::GetVertexBuffers() const
    {
        return vertexBuffers;
    }

    RDBufferViewPtr Mesh::GetIndexBuffer() const
    {
        return indexBuffer;
    }

    VkIndexType Mesh::GetIndexType() const
    {
        return indexType;
    }

}
