//
// Spot light component. Position and direction come from the entity transform.
//

#pragma once

#include <core/math/Vector3.h>
#include <framework/world/Component.h>

namespace sky::aurora {

    struct SpotLightData {
        Vector3 color          = {1.f, 1.f, 1.f};
        float   intensity      = 1.f;
        float   range          = 10.f;
        float   innerConeAngle = 0.f;
        float   outerConeAngle = 0.785398f; // radians (~45 deg)
    };

    class SpotLightComponent : public ComponentAdaptor<SpotLightData> {
    public:
        SpotLightComponent()           = default;
        ~SpotLightComponent() override = default;

        COMPONENT_RUNTIME_INFO(SpotLightComponent)

        static void Reflect(SerializationContext *context);

        void           SetColor(const Vector3 &value) { data.color = value; }
        const Vector3 &GetColor() const { return data.color; }

        void  SetIntensity(float value) { data.intensity = value; }
        float GetIntensity() const { return data.intensity; }

        void  SetRange(float value) { data.range = value; }
        float GetRange() const { return data.range; }

        void  SetInnerConeAngle(float value) { data.innerConeAngle = value; }
        float GetInnerConeAngle() const { return data.innerConeAngle; }

        void  SetOuterConeAngle(float value) { data.outerConeAngle = value; }
        float GetOuterConeAngle() const { return data.outerConeAngle; }
    };

} // namespace sky::aurora
