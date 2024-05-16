//
// Created by Zach Lee on 2023/9/16.
//

#pragma once

#include "framework/world/TransformComponent.h"

namespace sky {

    class SimpleRotateComponent : public ComponentBase {
    public:
        SimpleRotateComponent() = default;
        ~SimpleRotateComponent() override = default;

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

} // namespace sky