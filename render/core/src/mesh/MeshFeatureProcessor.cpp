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

    StaticMeshRenderer *MeshFeatureProcessor::CreateStaticMesh()
    {
        auto *renderer = new StaticMeshRenderer();
        renderer->AttachScene(scene);
        return staticMeshes.emplace_back(renderer).get();
    }

    void MeshFeatureProcessor::RemoveStaticMesh(StaticMeshRenderer *mesh)
    {
        staticMeshes.remove_if([mesh](const auto &val) {
            return mesh == val.get();
        });
    }

} // namespace sky
