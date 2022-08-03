//
// Created by Zach Lee on 2022/7/19.
//

#include <render/StaticMesh.h>
#include <render/RenderConstants.h>
#include <render/RenderView.h>
#include <render/RenderScene.h>
#include <vulkan/Util.h>

namespace sky {

    void StaticMesh::OnRender(RenderScene& scene)
    {
        for (auto& primitive : primitives) {
            auto& techs = primitive->GetTechniques();
            for (auto& tech : techs) {
                tech->setBinder->BindSet(0, objectSet->GetRHISet());
                tech->setBinder->BindSet(1, scene.GetSceneSet()->GetRHISet());
                tech->setBinder->BindSet(2, primitive->GetMaterialSet()->GetRHISet());
            }
        }
    }

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
        for (uint32_t i = 0; i < subMeshes.size(); ++i) {
            auto primitive = std::make_unique<RenderPrimitive>();
            primitive->SetMesh(mesh, i);
            primitive->SetVertexAssembly(vertexAssembly);
            primitive->SetObjectSet(objectSet);

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