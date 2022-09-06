//
// Created by Zach Lee on 2022/7/19.
//

#include <render/RenderConstants.h>
#include <render/RenderScene.h>
#include <render/RenderView.h>
#include <render/StaticMesh.h>
#include <vulkan/Util.h>

namespace sky {

    void StaticMesh::OnRender(RenderScene &scene)
    {
        for (auto &primitive : primitives) {
            auto &techs = primitive->GetTechniques();
            for (auto &tech : techs) {
                scene.FillSetBinder(*tech->setBinder);
                tech->setBinder->BindSet(1, objectSet->GetRHISet());
                tech->setBinder->SetOffset(1, 0, objectBuffer->GetDynamicOffset());
                tech->setBinder->BindSet(2, primitive->GetMaterialSet()->GetRHISet());
            }
        }
        RenderMesh::OnRender(scene);
    }

    void StaticMesh::SetMesh(const RDMeshPtr &m)
    {
        mesh           = m;
        auto &vbs      = mesh->GetVertexBuffers();
        auto  ib       = mesh->GetIndexBuffer();
        vertexAssembly = std::make_shared<drv::VertexAssembly>();

        for (auto &vb : vbs) {
            vertexAssembly->AddVertexBuffer(vb->GetBuffer()->GetRHIBuffer(), vb->GetOffset());
        }
        if (ib) {
            vertexAssembly->SetIndexBuffer(ib->GetBuffer()->GetRHIBuffer(), ib->GetOffset());
        }

        auto &subMeshes = mesh->GetSubMeshes();
        for (uint32_t i = 0; i < subMeshes.size(); ++i) {
            auto primitive = std::make_unique<RenderMeshPrimitive>();
            primitive->SetVertexAssembly(vertexAssembly);
            primitive->SetMesh(*mesh, i);

            primitives.emplace_back(std::move(primitive));
        }
    }

    void StaticMesh::OnGatherRenderPrimitives(RenderView &view)
    {
        for (auto &primitive : primitives) {
            view.AddRenderPrimitive(primitive.get());
        }
    }
} // namespace sky