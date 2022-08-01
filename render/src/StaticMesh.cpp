//
// Created by Zach Lee on 2022/7/19.
//

#include <render/StaticMesh.h>
#include <render/RenderConstants.h>
#include <render/RenderView.h>

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
            auto primitive = std::make_unique<RenderPrimitive>();
            primitive->SetAABB(subMesh.aabb);
            primitive->SetMaterial(subMesh.material);
            primitive->SetVertexAssembly(vertexAssembly);
            primitives.emplace_back(std::move(primitive));
        }

    }

    void StaticMesh::OnGatherRenderPrimitives(RenderView& view)
    {
        for (auto& primitive : primitives) {
            view.AddRenderPrimitive(primitive.get());
        }
    }
}