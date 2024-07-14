//
// Created by Zach Lee on 2023/9/2.
//

#include "SampleSceneCube.h"
#include <render/RenderWindow.h>
#include <render/adaptor/components/CameraComponent.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/adaptor/Util.h>
#include <render/geometry/GeometryRenderer.h>

#include "SimpleRotateComponent.h"

namespace sky {

    class SimpleGeometryComponent : public ComponentBase {
    public:
        SimpleGeometryComponent() = default;
        ~SimpleGeometryComponent() override = default;

        COMPONENT_RUNTIME_INFO(SimpleGeometryComponent)

        void OnActive() override
        {
            geometry = std::make_unique<GeometryRenderer>();
            geometry->Init();
            geometry->DrawAABB(AABB{{-1.f, -1.f, -1.f}, {1.f, 1.f, 1.f}});
            geometry->Upload();

            auto *scene = GetRenderSceneFromActor(actor);
            scene->AddPrimitive(geometry->GetPrimitive());
        }

        void Tick(float time) override
        {
            auto *ts = actor->GetComponent<TransformComponent>();
            geometry->UpdateTransform(ts->GetWorldMatrix());
        }

    protected:
        std::unique_ptr<GeometryRenderer> geometry;
    };

    bool SampleSceneCube::Start(sky::RenderWindow *window)
    {
        if (!SampleScene::Start(window)) {
            return false;
        }

//        cube = world->CreateActor("Cube");
//        cube->AddComponent<SimpleGeometryComponent>();
//        auto *sr = cube->AddComponent<SimpleRotateComponent>();
//        sr->SetAxis(VEC3_ONE);
//
//        camera = world->CreateActor("MainCamera");
//        auto *cc = camera->AddComponent<CameraComponent>();
//        cc->Perspective(0.01f, 100.f, 45.f);
//        cc->SetAspect(window->GetWidth(), window->GetHeight());
//        camera->GetComponent<TransformComponent>()->SetWorldTranslation(Vector3(0, 0, 5));
        return true;
    }

    void SampleSceneCube::Shutdown()
    {
        SampleScene::Shutdown();
    }

    void SampleSceneCube::Resize(uint32_t width, uint32_t height)
    {
        camera->GetComponent<CameraComponent>()->SetAspect(width, height);
    }
} // namespace sky
