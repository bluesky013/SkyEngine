//
// Created by Zach Lee on 2023/9/9.
//

#include <render/mesh/StaticMeshRenderer.h>
#include <render/mesh/MeshFeature.h>
#include <render/RenderBuiltinLayout.h>
#include <render/Renderer.h>

namespace sky {

    void StaticMeshRenderer::SetMesh(const RDMeshPtr &mesh_)
    {
        mesh = mesh_;

        if (!ubo) {
            ubo = std::make_shared<DynamicUniformBuffer>();
            ubo->Init(sizeof(InstanceLocal), Renderer::Get()->GetInflightFrameCount());
            ubo->Write(0, InstanceLocal{Matrix4::Identity(), Matrix4::Identity()});
            ubo->Upload();
        }

        if (!primitive) {
            auto *meshFeature = MeshFeature::Get();
            primitive = std::make_unique<RenderPrimitive>();
            primitive->instanceSet = meshFeature->RequestResourceGroup();
            primitive->instanceSet->BindDynamicUBO("ObjectInfo", ubo, 0);
            primitive->instanceSet->Update();
        }
    }

    void StaticMeshRenderer::UpdateTransform(const Matrix4 &matrix)
    {
        ubo->Write(0, matrix);
        ubo->Write(sizeof(Matrix4), matrix.InverseTranspose());
        ubo->Upload();
    }

} // namespace sky
