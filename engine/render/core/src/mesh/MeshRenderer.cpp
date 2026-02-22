//
// Created by Zach Lee on 2023/9/9.
//

#include <render/mesh/MeshRenderer.h>
#include <render/mesh/MeshFeature.h>
#include <render/RenderBuiltinLayout.h>
#include <render/Renderer.h>

namespace sky {

    static constexpr uint32_t MESH_GROUP_SIZE = 32;

    MeshRenderer::MeshRenderer(RenderScene *inScene) : scene(inScene)
    {
    }

    MeshRenderer::~MeshRenderer()
    {
        ResetPrimitive();
    }

    void MeshRenderer::InitUBO()
    {
        ubo = new DynamicUniformBuffer();
        ubo->Init(sizeof(InstanceLocal));
        ubo->WriteT(0, InstanceLocal{Matrix4::Identity(), Matrix4::Identity()});
        ubo->Upload();
    }

    void MeshRenderer::Init()
    {
        InitUBO();
        instanceSet = MeshFeature::Get()->RequestResourceGroup();
        instanceSet->BindDynamicUBO(Name("Local"), ubo, 0);
        instanceSet->Update();
    }

    void MeshRenderer::ResetPrimitive()
    {
        if (primitive) {
            scene->RemovePrimitive(primitive.get());
            primitive = nullptr;
            lodPrimitive = nullptr;
        }
    }

    void MeshRenderer::BuildPrimitive(const RDLodGroupPtr &inGroup)
    {
        lodPrimitive = new RenderLodPrimitive(inGroup);
        lodPrimitive->SetInstanceData(instanceSet);
        scene->AddPrimitive(lodPrimitive);
        primitive.reset(lodPrimitive);
    }

    void MeshRenderer::SetMeshLodGroup(const RDLodGroupPtr &inLodGroup)
    {
        ResetPrimitive();
        BuildPrimitive(inLodGroup);
    }

    void MeshRenderer::SetDebugFlags(const MeshDebugFlags& flag)
    {
    }

    void MeshRenderer::UpdateTransform(const Matrix4 &matrix)
    {
        if (primitive) {
            primitive->UpdateWorldBounds(matrix);
        }

        if (ubo) {
            ubo->WriteT(0, matrix);
            ubo->WriteT(sizeof(Matrix4), matrix.InverseTranspose());
            ubo->Upload();
        }
    }

} // namespace sky
