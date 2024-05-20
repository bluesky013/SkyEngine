//
// Created by Zach Lee on 2023/9/16.
//

#pragma once

#include "framework/world/TransformComponent.h"
#include "framework/controller/SimpleController.h"
#include "framework/interface/Interface.h"
#include "framework/interface/ISystem.h"

namespace sky {

    class SimpleRotateComponent : public ComponentBase {
    public:
        SimpleRotateComponent() = default;
        ~SimpleRotateComponent() override = default;

        COMPONENT_RUNTIME_INFO(SimpleRotateComponent)

        static void Reflect(SerializationContext *context)
        {
            context->Register<SimpleRotateComponent>("SimpleRotateComponent");
        }

        void Tick(float time) override
        {
            auto *ts = actor->GetComponent<TransformComponent>();
            ts->SetLocalRotation(Quaternion(angle, axis) * Quaternion(90 / 180.f * 3.14f, VEC3_Y));
            angle += 0.25f * time;
        }

        void SetAxis(const Vector3 &axis_)
        {
            axis = axis_;
        }

    private:
        float angle = 0.f;
        Vector3 axis = {0, 1, 0};
    };

    class SimpleCameraController : public ComponentBase {
    public:
        SimpleCameraController() = default;
        ~SimpleCameraController() override = default;

        COMPONENT_RUNTIME_INFO(SimpleCameraController)

        static void Reflect(SerializationContext *context)
        {
            context->Register<SimpleCameraController>("SimpleCameraController");
        }

        void OnActive() override
        {
            controller.BindWindow(Interface<ISystemNotify>::Get()->GetApi()->GetViewport());
        }

        void Tick(float time) override
        {
            auto *trans = actor->GetComponent<TransformComponent>();
            trans->SetLocalTransform(controller.Resolve(time, trans->GetLocalTransform()));
        }

    private:
        FirstPersonController controller;
    };

} // namespace sky