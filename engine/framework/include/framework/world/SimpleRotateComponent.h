//
// Created by blues on 2025/2/21.
//

#pragma once

#include <core/math/Transform.h>
#include <core/event/Event.h>
#include <framework/world/Component.h>
#include <framework/interface/ITransformEvent.h>

namespace sky {

    struct SimpleRotateData {
        float speed = 1.f;
    };

    class SimpleRotateComponent : public ComponentAdaptor<SimpleRotateData> {
    public:
        SimpleRotateComponent() = default;
        ~SimpleRotateComponent() = default;

        static void Reflect(SerializationContext *context);

        COMPONENT_RUNTIME_INFO(SimpleRotateComponent)

        void SetSpeed(float speed) { data.speed = speed; }
        float GetSpeed() const { return data.speed; }

        void Tick(float time) override;

        float angle = 0.f;
    };


} // namespace sky
