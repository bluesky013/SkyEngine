//
// Spot light component. Position and direction come from the entity transform.
//

#pragma once

#include <aurora/adaptor/components/AuroraComponentData.h>
#include <aurora/adaptor/renderer/SceneComponentEvent.h>

namespace sky::aurora {

    class SpotLightComponent : public SceneComponentAdaptor<SpotLightData> {
    public:
        SpotLightComponent()           = default;
        ~SpotLightComponent() override = default;

        COMPONENT_RUNTIME_INFO(SpotLightComponent)

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

        void SetRange(float value)
        {
            data.range = value;
        }
        float GetRange() const
        {
            return data.range;
        }

        void SetInnerConeAngle(float value)
        {
            data.innerConeAngle = value;
        }
        float GetInnerConeAngle() const
        {
            return data.innerConeAngle;
        }

        void SetOuterConeAngle(float value)
        {
            data.outerConeAngle = value;
        }
        float GetOuterConeAngle() const
        {
            return data.outerConeAngle;
        }
    };

} // namespace sky::aurora
