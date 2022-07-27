//
// Created by Zach Lee on 2022/7/18.
//

#include <render/features/StaticMeshFeature.h>

namespace sky {

    StaticMesh* StaticMeshFeature::Create()
    {
        meshes.emplace_back(new StaticMesh);
        return meshes.back().get();
    }

    void StaticMeshFeature::Release(StaticMesh* mesh)
    {
        auto iter = std::find_if(meshes.begin(), meshes.end(), [mesh](StaticMeshPtr& rhs) {
            return mesh == rhs.get();
        });
        if (iter != meshes.end()) {
            meshes.erase(iter);
        }
    }

    void StaticMeshFeature::GatherRenderItem(RenderScene& scene)
    {

    }

    void StaticMeshFeature::OnRender(RenderScene& scene)
    {

    }
}