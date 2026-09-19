//
// Directional light component. Direction comes from the entity transform.
//

#pragma once

#include <core/math/Vector3.h>
#include <framework/world/Component.h>

namespace sky::aurora {

    struct DirectLightData {
        Vector3 color     = {1.f, 1.f, 1.f};
        float   intensity = 1.f;
        bool    castShadow = true;
    };

    class DirectLightComponent : public ComponentAdaptor<DirectLightData> {
    public:
        DirectLightComponent()           = default;
        ~DirectLightComponent() override = default;

        COMPONENT_RUNTIME_INFO(DirectLightComponent)

        static void Reflect(SerializationContext *context);

        void           SetColor(const Vector3 &value) { data.color = value; }
        const Vector3 &GetColor() const { return data.color; }

        void  SetIntensity(float value) { data.intensity = value; }
        float GetIntensity() const { return data.intensity; }

        void SetCastShadow(bool value) { data.castShadow = value; }
        bool GetCastShadow() const { return data.castShadow; }
    };

} // namespace sky::aurora
