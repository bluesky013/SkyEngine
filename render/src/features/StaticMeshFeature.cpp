//
// Created by Zach Lee on 2022/7/18.
//

#include <render/RenderScene.h>
#include <render/features/StaticMeshFeature.h>

namespace sky {

    StaticMesh *StaticMeshFeature::Create()
    {
        meshes.emplace_back(new StaticMesh());
        auto mesh = static_cast<StaticMesh *>(meshes.back().get());
        mesh->AddToScene(scene);
        return mesh;
    }

    void StaticMeshFeature::Release(StaticMesh *mesh)
    {
        if (mesh == nullptr) {
            return;
        }
        mesh->RemoveFromScene(scene);

        meshes.erase(std::remove_if(meshes.begin(), meshes.end(), [mesh](auto &ptr) { return ptr.get() == mesh; }), meshes.end());
    }

    void StaticMeshFeature::OnRender()
    {
        for (auto &mesh : meshes) {
            mesh->OnRender(scene);
        }
    }

    void StaticMeshFeature::GatherRenderPrimitives()
    {
        auto &views = scene.GetViews();
        for (auto &view : views) {
            for (auto &mesh : meshes) {
                mesh->OnGatherRenderPrimitives(*view);
            }
        }
    }
} // namespace sky