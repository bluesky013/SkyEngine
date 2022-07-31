//
// Created by Zach Lee on 2022/7/19.
//

#include <render/StaticMesh.h>
#include <render/RenderConstants.h>

namespace sky {

    void StaticMesh::SetMesh(RDMeshPtr m)
    {
        mesh = m;

        auto& vbs = mesh->GetVertexBuffers();
        auto ib = mesh->GetIndexBuffer();
        vertexAssembly = std::make_shared<drv::VertexAssembly>();

        for (auto& vb : vbs) {
            vertexAssembly->AddVertexBuffer(vb->GetBuffer()->GetRHIBuffer(), vb->GetOffset());
        }
        if (ib) {
            vertexAssembly->SetIndexBuffer(ib->GetBuffer()->GetRHIBuffer(), ib->GetOffset());
        }

        auto& subMeshes = mesh->GetSubMeshes();
        for (auto& subMesh : subMeshes) {
            auto primitive = std::make_shared<RenderPrimitive>();
            primitive->SetViewMask(MAIN_CAMERA_TAG);
            primitive->SetAABB(subMesh.aabb);
            primitive->SetMaterial(subMesh.material);
            primitive->SetVertexAssembly(vertexAssembly);
        }

    }

    void StaticMesh::Setup()
    {

    }

}