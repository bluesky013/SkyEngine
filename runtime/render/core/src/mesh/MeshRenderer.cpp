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

    void MeshRenderer::Tick()
    {
        for (auto &prim : primitives) {
            prim->isReady = mesh->IsReady();
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
            TechniqueInstance inst = {tech, mat};
            inst.shaderOption = new ShaderOption();
            tech->Process(primitive->vertexFlags, inst.shaderOption);
            primitive->techniques.emplace_back(inst);
        }

        primitive->batchSet = mat->GetResourceGroup();
    }

    void MeshRenderer::SetMesh(const RDMeshPtr &mesh_)
    {
        mesh = mesh_;
        mesh->Upload();

        if (!ubo) {
            PrepareUBO();
        }

        uint32_t index = 0;
        auto *meshFeature = MeshFeature::Get();
        for (const auto &sub : mesh->GetSubMeshes()) {
            auto &primitive = primitives.emplace_back(std::make_unique<RenderPrimitive>());
            primitive->instanceSet = RequestResourceGroup(meshFeature);
            primitive->instanceSet->BindDynamicUBO("localData", ubo, 0);
            primitive->instanceSet->Update();

            primitive->localBound = sub.aabb;
            primitive->geometry   = mesh->GetGeometry();

            if (primitive->geometry->attributeSemantics.TestBit(VertexSemanticFlagBit::HAS_SKIN)) {
                primitive->vertexFlags |= RenderVertexFlagBit::SKIN;
            }

            primitive->args.emplace_back(rhi::CmdDrawIndexed {
                sub.indexCount,
                1,
                sub.firstIndex,
                static_cast<int32_t>(sub.firstVertex),
                0
            });

            SetMaterial(sub.material, index++);
            scene->AddPrimitive(primitive.get());
        }
    }

    void MeshRenderer::UpdateTransform(const Matrix4 &matrix)
    {
        ubo->WriteT(0, matrix);
        ubo->WriteT(sizeof(Matrix4), matrix.InverseTranspose());
        ubo->Upload();

        for (auto &prim : primitives) {
            prim->worldBound = AABB::Transform(prim->localBound, matrix);
        }
    }

    void MeshRenderer::PrepareUBO()
    {
        ubo = new DynamicUniformBuffer();
        ubo->Init(sizeof(InstanceLocal));
        ubo->WriteT(0, InstanceLocal{Matrix4::Identity(), Matrix4::Identity()});
        ubo->Upload();
    }

    RDResourceGroupPtr MeshRenderer::RequestResourceGroup(MeshFeature *feature)
    {
        return feature->RequestResourceGroup();
    }

} // namespace sky
