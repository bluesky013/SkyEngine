//
// Camera component: framework-only camera parameters (no asset).
//

#pragma once

#include <framework/world/Component.h>

namespace sky::aurora {

    struct CameraComponentData {
        float fov   = 0.785398f; // radians (~45 deg)
        float nearZ = 0.1f;
        float farZ  = 1000.f;
    };

    class CameraComponent : public ComponentAdaptor<CameraComponentData> {
    public:
        CameraComponent()           = default;
        ~CameraComponent() override = default;

        COMPONENT_RUNTIME_INFO(CameraComponent)

        static void Reflect(SerializationContext *context);

        void  SetFov(float value) { data.fov = value; }
        float GetFov() const { return data.fov; }

        void  SetNearZ(float value) { data.nearZ = value; }
        float GetNearZ() const { return data.nearZ; }

        void  SetFarZ(float value) { data.farZ = value; }
        float GetFarZ() const { return data.farZ; }
    };

} // namespace sky::aurora
