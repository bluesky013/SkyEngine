//
// Created by Zach Lee on 2023/9/16.
//

#pragma once

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
            ts->SetLocalRotation(Quaternion(angle, axis) * Quaternion(90 / 180.f * 3.14f, VEC3_X));
            angle += 0.5f * time;
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