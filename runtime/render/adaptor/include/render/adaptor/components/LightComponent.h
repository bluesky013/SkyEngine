//
// Created by Zach Lee on 2023/2/28.
//

#pragma once

#include <core/math/Color.h>
#include <framework/world/Component.h>
#include <render/light/LightFeatureProcessor.h>

namespace sky {

    class LightComponent : public ComponentBase {
    public:
        LightComponent() = default;
        ~LightComponent() override = default;

        COMPONENT_RUNTIME_INFO(LightComponent)

        static void Reflect(SerializationContext *context);

        void Tick(float time) override;
        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

        void SaveJson(JsonOutputArchive &ar) const override;
        void LoadJson(JsonInputArchive &ar) override;

    private:
        Color lightColor;

        DirectLight *light = nullptr;
    };

} // namespace sky




