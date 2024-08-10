//
// Created by Zach Lee on 2023/9/9.
//

#include <render/mesh/MeshFeatureProcessor.h>

namespace sky {

    void MeshFeatureProcessor::Tick(float time)
    {

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
