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

    struct PointLightData {
        ColorRGB color = ColorRGB{1.f, 1.f, 1.f};
        float intensity = 1.f;
        float range = PointLight::DEFAULT_RANGE;
    };

    class PointLightComponent
        : public ComponentAdaptor<PointLightData>
        , public ITransformEvent {
    public:
        PointLightComponent() = default;
        ~PointLightComponent() override = default;

        COMPONENT_RUNTIME_INFO(PointLightComponent)

        static void Reflect(SerializationContext *context);

        void SetColor(const ColorRGB &color);
        const ColorRGB &GetColor() const { return data.color; }

        void SetIntensity(float intensity);
        float GetIntensity() const { return data.intensity; }

        void SetRange(float range);
        float GetRange() const { return data.range; }

        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

    private:
        void OnTransformChanged(const Transform& global, const Transform& local) override;
        void UpdateData(const Transform& global);

        PointLight *light = nullptr;
        EventBinder<ITransformEvent> binder;
    };

    struct SpotLightComponentData {
        ColorRGB color = ColorRGB{1.f, 1.f, 1.f};
        float intensity = 1.f;
        float range = SpotLight::DEFAULT_RANGE;
        float innerAngle = SpotLight::DEFAULT_INNER_ANGLE;
        float outerAngle = SpotLight::DEFAULT_OUTER_ANGLE;
    };

    class SpotLightComponent
        : public ComponentAdaptor<SpotLightComponentData>
        , public ITransformEvent {
    public:
        SpotLightComponent() = default;
        ~SpotLightComponent() override = default;

        COMPONENT_RUNTIME_INFO(SpotLightComponent)

        static void Reflect(SerializationContext *context);

        void SetColor(const ColorRGB &color);
        const ColorRGB &GetColor() const { return data.color; }

        void SetIntensity(float intensity);
        float GetIntensity() const { return data.intensity; }

        void SetRange(float range);
        float GetRange() const { return data.range; }

        void SetInnerAngle(float angle);
        float GetInnerAngle() const { return data.innerAngle; }

        void SetOuterAngle(float angle);
        float GetOuterAngle() const { return data.outerAngle; }

        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

    private:
        void OnTransformChanged(const Transform& global, const Transform& local) override;
        void UpdateData(const Transform& global);

        SpotLight *light = nullptr;
        EventBinder<ITransformEvent> binder;
    };

} // namespace sky




