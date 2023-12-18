//
// Created by Zach Lee on 2023/9/16.
//

#include "SampleMesh.h"
#include <render/RenderWindow.h>
#include <render/adaptor/components/CameraComponent.h>
#include <render/adaptor/components/MeshRenderer.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/geometry/GeometryRenderer.h>

#include <render/mesh/GridRenderer.h>

#include "SimpleRotateComponent.h"

namespace sky {

    bool SampleMesh::Start(sky::RenderWindow *window)
    {
        if (!SampleScene::Start(window)) {
            return false;
        }

        meshObj = world->CreateGameObject("Cube");
        meshObj->AddComponent<SimpleRotateComponent>();

        auto *mesh = meshObj->AddComponent<MeshRenderer>();
        mesh->SetMesh(AssetManager::Get()->LoadAsset<Mesh>("DamagedHelmet/DamagedHelmet_mesh_0.mesh"));

//        auto mat = AssetManager::Get()->LoadAsset<MaterialInstance>("materials/floor.mati")->CreateInstance();
//        auto floor = GridRenderer().SetUp({512}).BuildMesh(mat);
//        mesh->SetMesh(floor);

        camera = world->CreateGameObject("MainCamera");
        auto *cc = camera->AddComponent<CameraComponent>();
        cc->Perspective(0.01f, 100.f, 45.f / 180.f * 3.14f);
        cc->SetAspect(window->GetWidth(), window->GetHeight());
        camera->GetComponent<TransformComponent>()->SetWorldTranslation(Vector3(0, 1, 5));

        return true;
    }

    void SampleMesh::Shutdown()
    {
        SampleScene::Shutdown();
    }

    void SampleMesh::Resize(uint32_t width, uint32_t height)
    {
        camera->GetComponent<CameraComponent>()->SetAspect(width, height);
    }
} // namespace sky