//
// Created by Zach Lee on 2023/9/9.
//

#include <render/mesh/MeshRenderer.h>
#include <render/mesh/MeshFeature.h>
#include <render/RenderBuiltinLayout.h>
#include <render/Renderer.h>
#include <render/RHI.h>

namespace sky {

    MeshRenderer::~MeshRenderer()
    {
        for (auto &prim : primitives) {
            scene->RemovePrimitive(prim.get());
        }
    }

    void MeshRenderer::AttachScene(RenderScene *scn)
    {
        scene = scn;
    }

    void MeshRenderer::SetMaterial(const RDMaterialInstancePtr &mat, uint32_t subMesh)
    {
        auto &primitive = primitives[subMesh];
        primitive->techniques.clear();

        const auto &techniques = mat->GetMaterial()->GetGfxTechniques();
        primitive->techniques.reserve(techniques.size());
        for (const auto &tech : techniques) {
            primitive->techniques.emplace_back(TechniqueInstance{tech});
        }

        primitive->batchSet = mat->GetResourceGroup();
    }

    void MeshRenderer::SetMesh(const RDMeshPtr &mesh_)
    {
        mesh = mesh_;

        if (!ubo) {
            PrepareUBO();
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

            primitive->instanceSet = RequestResourceGroup(meshFeature);
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

    void MeshRenderer::UpdateTransform(const Matrix4 &matrix)
    {
        ubo->Write(0, matrix);
        ubo->Write(sizeof(Matrix4), matrix.InverseTranspose());
        ubo->Upload();
    }

    void MeshRenderer::PrepareUBO()
    {
        ubo = new DynamicUniformBuffer();
        ubo->Init(sizeof(InstanceLocal), Renderer::Get()->GetInflightFrameCount());
        ubo->Write(0, InstanceLocal{Matrix4::Identity(), Matrix4::Identity()});
        ubo->Upload();
    }

    RDResourceGroupPtr MeshRenderer::RequestResourceGroup(MeshFeature *feature)
    {
        return feature->RequestResourceGroup();
    }

} // namespace sky
