//
// Created by Zach Lee on 2023/9/2.
//

#include "SampleSceneCube.h"
#include <render/RenderWindow.h>
#include <render/adaptor/components/MeshRenderer.h>
#include <render/adaptor/components/CameraComponent.h>
#include <framework/world/TransformComponent.h>

namespace sky {

    class SimpleRotateComponent : public Component {
    public:
        SimpleRotateComponent() = default;
        ~SimpleRotateComponent() override = default;

        TYPE_RTTI_WITH_VT(SimpleRotateComponent)

        void OnTick(float time) override
        {
            auto *ts = object->GetComponent<TransformComponent>();
            ts->SetLocalRotation(Quaternion(angle, Vector3(1, 1, 1)));
            angle += 0.01f * time;
        }

    private:
        float angle = 0.f;
    };

    bool SampleSceneCube::Start(sky::RenderWindow *window)
    {
        if (!SampleScene::Start(window)) {
            return false;
        }

        cube = world->CreateGameObject("Cube");
        auto *mc = cube->AddComponent<MeshRenderer>();
        auto meshAsset = AssetManager::Get()->LoadAsset<Mesh>("box/box_mesh_0.mesh");
        mc->SetMesh(meshAsset);

        cube->AddComponent<SimpleRotateComponent>();

        camera = world->CreateGameObject("MainCamera");
        auto *cc = camera->AddComponent<CameraComponent>();
        cc->Perspective(0.01f, 100.f, 45.f / 180.f * 3.14f);
        cc->SetAspect(window->GetWidth(), window->GetHeight());
        return true;
    }

    void SampleSceneCube::Resize(uint32_t width, uint32_t height)
    {
        camera->GetComponent<CameraComponent>()->SetAspect(width, height);
    }
} // namespace sky
