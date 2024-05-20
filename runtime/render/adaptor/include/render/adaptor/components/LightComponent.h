//
// Created by Zach Lee on 2023/2/28.
//

#pragma once

#include <core/math/Color.h>
#include <framework/world/Component.h>

namespace sky {

    class LightComponent : public ComponentBase {
    public:
        LightComponent() = default;
        ~LightComponent() override = default;

        COMPONENT_RUNTIME_INFO(LightComponent)

        static void Reflect(SerializationContext *context);

        void SaveJson(JsonOutputArchive &ar) const override;
        void LoadJson(JsonInputArchive &ar) override;

    private:
        Color lightColor;
    };

} // namespace sky




