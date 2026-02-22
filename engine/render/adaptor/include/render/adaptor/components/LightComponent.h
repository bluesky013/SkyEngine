//
// Created by Zach Lee on 2023/2/28.
//

#pragma once

#include <core/math/Color.h>
#include <framework/world/Component.h>
#include <framework/interface/ITransformEvent.h>
#include <render/light/LightFeatureProcessor.h>

namespace sky {

    struct MainLightData {
        ColorRGB color = ColorRGB{1.f, 1.f, 1.f};
        float intensity = 1.f;
        bool castShadow = true;
    };

    class MainDirectLightComponent
        : public ComponentAdaptor<MainLightData>
        , public ITransformEvent {
    public:
        MainDirectLightComponent() = default;
        ~MainDirectLightComponent() override = default;

        COMPONENT_RUNTIME_INFO(MainDirectLightComponent)

        static void Reflect(SerializationContext *context);

        void SetColor(const ColorRGB &color);
        const ColorRGB &GetColor() const { return data.color; }

        void SetIntensity(float intensity);
        float GetIntensity() const { return data.intensity; }

        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

    private:
        void OnTransformChanged(const Transform& global, const Transform& local) override;
        void UpdateData(const Transform& global);

        MainDirectLight *light = nullptr;
        EventBinder<ITransformEvent> binder;
    };

} // namespace sky




