//
// Created by Zach Lee on 2023/9/9.
//

#include <render/mesh/StaticMeshRenderer.h>
#include <render/mesh/MeshFeature.h>
#include <render/RenderBuiltinLayout.h>
#include <render/Renderer.h>
#include <render/RHI.h>
#include <render/VertexDescLibrary.h>

namespace sky {

    StaticMeshRenderer::~StaticMeshRenderer()
    {
        for (auto &prim : primitives) {
            scene->RemovePrimitive(prim.get());
        }
    }

    void StaticMeshRenderer::AttachScene(RenderScene *scn)
    {
        scene = scn;
    }

    void StaticMeshRenderer::SetMaterial(const RDMaterialInstancePtr &mat, uint32_t subMesh)
    {
        mesh->SetMaterial(mat, subMesh);

        auto &primitive = primitives[subMesh];
        primitive->techniques.clear();

        const auto &techniques = mat->GetMaterial()->GetGfxTechniques();
        primitive->techniques.reserve(techniques.size());
        for (const auto &tech : techniques) {
            primitive->techniques.emplace_back(TechniqueInstance{tech});
        }

        primitive->batchSet = mat->GetResourceGroup();
    }

    void StaticMeshRenderer::SetMesh(const RDMeshPtr &mesh_)
    {
        mesh = mesh_;

        if (!ubo) {
            ubo = std::make_shared<DynamicUniformBuffer>();
            ubo->Init(sizeof(InstanceLocal), Renderer::Get()->GetInflightFrameCount());
            ubo->Write(0, InstanceLocal{Matrix4::Identity(), Matrix4::Identity()});
            ubo->Upload();
        }

        if (!va) {
            rhi::VertexAssembly::Descriptor desc = {};
            const auto &vbs = mesh->GetVertexBuffers();
            desc.vertexBuffers.resize(vbs.size());
            for (uint32_t i = 0; i < vbs.size(); ++i) {
                desc.vertexBuffers[i] = vbs[i]->GetRHIBuffer()->CreateView({0, vbs[i]->GetRange()});
            }
            desc.indexBuffer = mesh->GetIndexBuffer()->GetRHIBuffer()->CreateView({0, mesh->GetIndexBuffer()->GetRange()});
            desc.indexType = mesh->GetIndexType();
            va = RHI::Get()->GetDevice()->CreateVertexAssembly(desc);
        }

        uint32_t index = 0;
        auto *meshFeature = MeshFeature::Get();
        for (const auto &sub : mesh->GetSubMeshes()) {
            auto &primitive = primitives.emplace_back(std::make_unique<RenderPrimitive>());
            SetMaterial(sub.material, index++);

            primitive->instanceSet = meshFeature->RequestResourceGroup();
            primitive->instanceSet->BindDynamicUBO("localData", ubo, 0);
            primitive->instanceSet->Update();

            primitive->boundingBox = sub.aabb;
            primitive->va = va;
            primitive->args.emplace_back(rhi::CmdDrawIndexed {
                sub.indexCount,
                1,
                sub.firstIndex,
                static_cast<int32_t>(sub.firstVertex),
                0
            });

            scene->AddPrimitive(primitive.get());
        }
    }

    void StaticMeshRenderer::UpdateTransform(const Matrix4 &matrix)
    {
        ubo->Write(0, matrix);
        ubo->Write(sizeof(Matrix4), matrix.InverseTranspose());
        ubo->Upload();
    }

} // namespace sky
