//
// Aurora light component: framework view over the aurora scene Light data
// (no asset reference).
//

#pragma once

#include <aurora/scene/SceneTypes.h>
#include <framework/world/Component.h>

namespace sky {

    class AuroraLightComponent : public ComponentAdaptor<sky::aurora::Light> {
    public:
        AuroraLightComponent()           = default;
        ~AuroraLightComponent() override = default;

        COMPONENT_RUNTIME_INFO(AuroraLightComponent)

        static void Reflect(SerializationContext *context);
    };

} // namespace sky
