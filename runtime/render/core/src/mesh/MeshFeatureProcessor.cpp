//
// Created by Zach Lee on 2023/9/9.
//

#include <render/mesh/MeshFeatureProcessor.h>
#include <render/RenderScene.h>
#include <render/SceneView.h>
#include <core/math/MathUtil.h>

namespace sky {

    void MeshFeatureProcessor::Tick(float time)
    {
        UpdateLod();

        for (auto &mesh : staticMeshes) {
            mesh->Tick();
        }

        for (auto &mesh : skeletonMeshes) {
            mesh->Tick();
        }
    }

    void MeshFeatureProcessor::UpdateLod()
    {
        auto *view = scene->GetSceneView(mainViewName);
        if (view == nullptr) {
            return;
        }

        const auto &worldMat = view->GetWorld();
        Vector3 viewPos(worldMat[3][0], worldMat[3][1], worldMat[3][2]);

        const auto &projMat = view->GetProject();
        float fov = 2.0f * std::atan(1.0f / projMat[1][1]);
        float screenHeight = 1080.0f;

        for (auto &mesh : staticMeshes) {
            mesh->UpdateLod(viewPos, fov, screenHeight);
        }
    }

    void MeshFeatureProcessor::Render(rdg::RenderGraph &rdg)
    {
    }

    MeshRenderer *MeshFeatureProcessor::CreateStaticMesh()
    {
        auto *renderer = new MeshRenderer();
        renderer->AttachScene(scene);
        return staticMeshes.emplace_back(renderer).get();
    }

    void MeshFeatureProcessor::RemoveStaticMesh(MeshRenderer *mesh)
    {
        staticMeshes.remove_if([mesh](const auto &val) {
            return mesh == val.get();
        });
    }

    SkeletonMeshRenderer *MeshFeatureProcessor::CreateSkeletonMesh()
    {
        auto *renderer = new SkeletonMeshRenderer();
        renderer->AttachScene(scene);
        return skeletonMeshes.emplace_back(renderer).get();
    }

    void MeshFeatureProcessor::RemoveSkeletonMesh(SkeletonMeshRenderer *mesh)
    {
        skeletonMeshes.remove_if([mesh](const auto &val) {
            return mesh == val.get();
        });
    }

} // namespace sky
