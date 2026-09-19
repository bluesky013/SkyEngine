//
// Point light component. Position comes from the entity transform.
//

#pragma once

#include <core/math/Vector3.h>
#include <framework/world/Component.h>

namespace sky::aurora {

    struct PointLightData {
        Vector3 color     = {1.f, 1.f, 1.f};
        float   intensity = 1.f;
        float   range     = 10.f;
    };

    class PointLightComponent : public ComponentAdaptor<PointLightData> {
    public:
        PointLightComponent()           = default;
        ~PointLightComponent() override = default;

        COMPONENT_RUNTIME_INFO(PointLightComponent)

        static void Reflect(SerializationContext *context);

        void           SetColor(const Vector3 &value) { data.color = value; }
        const Vector3 &GetColor() const { return data.color; }

        void  SetIntensity(float value) { data.intensity = value; }
        float GetIntensity() const { return data.intensity; }

        void  SetRange(float value) { data.range = value; }
        float GetRange() const { return data.range; }
    };

} // namespace sky::aurora
