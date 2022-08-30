//
// Created by Zach Lee on 2022/8/1.
//

#include <render/RenderEncoder.h>
#include <render/RenderMeshPrimtive.h>
#include <render/resources/Mesh.h>

namespace sky {

    void RenderMeshPrimitive::SetMesh(Mesh &mesh, uint32_t index)
    {
        subMeshIndex   = index;
        auto &subMesh  = mesh.GetSubMesh(subMeshIndex);
        auto &material = subMesh.material;
        matSet         = material->GetMaterialSet();
        args           = mesh.BuildDrawArgs(index);

        {
            auto &techniques = material->GetGraphicTechniques();
            for (auto &tech : techniques) {
                auto proxy         = new GraphicsTechniqueProxy();
                proxy->setBinder   = tech->CreateSetBinder();
                proxy->assembly    = vertexAssembly;
                proxy->vertexInput = mesh.BuildVertexInput(*tech->GetShaderTable()->GetVS());
                proxy->args        = args;
                proxy->drawTag |= tech->GetDrawTag();
                proxy->pso = tech->AcquirePso(proxy->vertexInput);

                SetViewTag(tech->GetViewTag());
                graphicTechniques.emplace_back(proxy);
            }
        }
    }

    void RenderMeshPrimitive::Encode(RenderRasterEncoder *encoder)
    {
        for (auto &tech : graphicTechniques) {
            if ((encoder->GetDrawTag() & tech->drawTag) == 0) {
                continue;
            }
            drv::DrawItem item;
            item.pso             = tech->pso;
            item.vertexAssembly  = tech->assembly;
            item.drawArgs        = tech->args;
            item.shaderResources = tech->setBinder;
            encoder->Emplace(item);
        }
    }
} // namespace sky