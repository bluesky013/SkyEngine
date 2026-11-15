//
// Directional light component. Direction comes from the entity transform.
//

#pragma once

#include <aurora/adaptor/components/AuroraComponentData.h>
#include <aurora/adaptor/renderer/SceneComponentEvent.h>

namespace sky::aurora {

    class DirectLightComponent : public SceneComponentAdaptor<DirectLightData> {
    public:
        DirectLightComponent()           = default;
        ~DirectLightComponent() override = default;

        COMPONENT_RUNTIME_INFO(DirectLightComponent)

        static void Reflect(SerializationContext *context);

        void SetColor(const Vector3 &value)
        {
            data.color = value;
        }
        const Vector3 &GetColor() const
        {
            return data.color;
        }

        void SetIntensity(float value)
        {
            data.intensity = value;
        }
        float GetIntensity() const
        {
            return data.intensity;
        }

        void SetCastShadow(bool value)
        {
            data.castShadow = value;
        }
        bool GetCastShadow() const
        {
            return data.castShadow;
        }
    };

} // namespace sky::aurora
