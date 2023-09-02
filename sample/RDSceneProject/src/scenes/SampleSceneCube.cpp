//
// Created by Zach Lee on 2023/9/2.
//

#include "SampleSceneCube.h"
#include <render/adaptor/components/MeshRenderer.h>
#include <render/adaptor/components/CameraComponent.h>
#include <framework/world/TransformComponent.h>

namespace sky {

    class SimpleRotateComponent : public Component {
    public:
        SimpleRotateComponent() = default;
        ~SimpleRotateComponent() override = default;

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
        cube->AddComponent<MeshRenderer>();

        camera = world->CreateGameObject("MainCamera");
        auto *cc = camera->AddComponent<CameraComponent>();
        cc->Perspective(0.01f, 100.f, 45.f / 180.f * 3.14f, 1.f);
        return true;
    }
} // namespace sky
