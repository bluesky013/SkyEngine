//
// Created by Zach Lee on 2022/8/1.
//

#include <render/RenderPrimtive.h>

namespace sky {

    void RenderPrimitive::SetMesh(RDMeshPtr& value, uint32_t index)
    {
        mesh = value;
        subMeshIndex = index;
        auto& subMesh = mesh->GetSubMesh(subMeshIndex);
        auto& material = subMesh.material;
        matSet = material->GetMaterialSet();
        args = mesh->BuildDrawArgs(index);

        {
            auto& techniques = material->GetGraphicTechniques();
            for (auto& tech : techniques) {
                auto proxy = new GraphicsTechniqueProxy();
                proxy->gfxTechnique = tech;
                proxy->setBinder = tech->CreateSetBinder();
                proxy->assembly = vertexAssembly;
                proxy->vertexInput = mesh->BuildVertexInput(*tech->GetShaderTable()->GetVS());
                proxy->args = &args;
                proxy->drawTag |= tech->GetDrawTag();
                proxy->pso = tech->AcquirePso(proxy->vertexInput);

                SetViewTag(tech->GetViewTag());
                graphicTechniques.emplace_back(proxy);
            }
        }

    }

}