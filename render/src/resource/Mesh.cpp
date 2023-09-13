//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Mesh.h>

namespace sky {

    void Mesh::AddSubMesh(const SubMesh &sub)
    {
        subMeshes.emplace_back(sub);
    }

    void Mesh::AddVertexBuffer(const RDBufferPtr &vb)
    {
        vertexBuffers.emplace_back(vb);
    }

    void Mesh::SetIndexBuffer(const RDBufferPtr &ib)
    {
        indexBuffer = ib;
    }


} // namespace sky