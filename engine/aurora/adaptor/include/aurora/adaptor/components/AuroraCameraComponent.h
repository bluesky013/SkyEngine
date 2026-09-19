//
// Aurora camera component: framework-only camera parameters (no asset).
//

#pragma once

#include <framework/world/Component.h>

namespace sky {

    struct AuroraCameraData {
        float fov   = 0.785398f; // radians (~45 deg)
        float nearZ = 0.1f;
        float farZ  = 1000.f;
    };

    class AuroraCameraComponent : public ComponentAdaptor<AuroraCameraData> {
    public:
        AuroraCameraComponent()           = default;
        ~AuroraCameraComponent() override = default;

        COMPONENT_RUNTIME_INFO(AuroraCameraComponent)

        static void Reflect(SerializationContext *context);

        void  SetFov(float value) { data.fov = value; }
        float GetFov() const { return data.fov; }

        void  SetNearZ(float value) { data.nearZ = value; }
        float GetNearZ() const { return data.nearZ; }

        void  SetFarZ(float value) { data.farZ = value; }
        float GetFarZ() const { return data.farZ; }
    };

} // namespace sky
