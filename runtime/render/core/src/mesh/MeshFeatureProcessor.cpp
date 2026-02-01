//
// Created by Zach Lee on 2023/9/9.
//

#include <render/mesh/MeshFeatureProcessor.h>

namespace sky {

    void MeshFeatureProcessor::Tick(float time)
    {
        for (auto &mesh : staticMeshes) {
            mesh->Tick();
        }

        for (auto &mesh : skeletonMeshes) {
            mesh->Tick();
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

    SkeletalMeshRenderer *MeshFeatureProcessor::CreateSkeletalMesh()
    {
        auto *renderer = new SkeletalMeshRenderer();
        renderer->AttachScene(scene);
        return skeletonMeshes.emplace_back(renderer).get();
    }

    void MeshFeatureProcessor::RemoveSkeletalMesh(SkeletalMeshRenderer *mesh)
    {
        skeletonMeshes.remove_if([mesh](const auto &val) {
            return mesh == val.get();
        });
    }

} // namespace sky
