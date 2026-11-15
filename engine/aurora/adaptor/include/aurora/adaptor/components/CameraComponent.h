//
// Camera component: framework-only camera parameters (no asset).
//

#pragma once

#include <aurora/adaptor/components/AuroraComponentData.h>
#include <aurora/adaptor/renderer/SceneComponentEvent.h>

namespace sky::aurora {

    class CameraComponent : public SceneComponentAdaptor<CameraComponentData> {
    public:
        CameraComponent()           = default;
        ~CameraComponent() override = default;

        COMPONENT_RUNTIME_INFO(CameraComponent)

        static void Reflect(SerializationContext *context);

        void SetFov(float value)
        {
            data.fov = value;
        }
        float GetFov() const
        {
            return data.fov;
        }

        void SetNearZ(float value)
        {
            data.nearZ = value;
        }
        float GetNearZ() const
        {
            return data.nearZ;
        }

        void SetFarZ(float value)
        {
            data.farZ = value;
        }
        float GetFarZ() const
        {
            return data.farZ;
        }
    };

} // namespace sky::aurora
