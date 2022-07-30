//
// Created by Zach Lee on 2022/7/18.
//

#include <render/features/StaticMeshFeature.h>

namespace sky {

    StaticMesh* StaticMeshFeature::Create()
    {
        meshes.emplace_back(new StaticMesh());
        return static_cast<StaticMesh*>(meshes.back().get());
    }

    void StaticMeshFeature::Release(StaticMesh* mesh)
    {
        meshes.erase(std::remove_if(meshes.begin(), meshes.end(),[mesh](auto& ptr) {
            return ptr.get() == mesh;
        }), meshes.end());
    }

    void StaticMeshFeature::GatherRenderItem(RenderScene& scene)
    {

    }
}