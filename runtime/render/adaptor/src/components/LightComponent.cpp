//
// Created by Zach Lee on 2023/2/28.
//

#include <render/adaptor/components/LightComponent.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/world/TransformComponent.h>
#include <render/light/LightFeatureProcessor.h>
#include <render/adaptor/Util.h>
#include <core/math/MathUtil.h>

namespace sky {

    void MainDirectLightComponent::Reflect(SerializationContext *context)
    {
        context->Register<MainLightData>("MainLightData")
            .Member<&MainLightData::color>("Color")
            .Member<&MainLightData::intensity>("Intensity");

        REGISTER_BEGIN(DirectLightComponent, context)
            REGISTER_MEMBER(LightColor, SetColor, GetColor)
            REGISTER_MEMBER(Intensity, SetIntensity, GetIntensity);
    }

    void MainDirectLightComponent::SetColor(const ColorRGB &color)
    {
        data.color = ColorRGB(color);

        if (light != nullptr) {
            light->SetColor(data.color);
        }
    }

    void MainDirectLightComponent::SetIntensity(float intensity)
    {
        data.intensity = intensity;

        if (light != nullptr) {
            light->SetIntensity(intensity);
        }
    }

    void MainDirectLightComponent::OnAttachToWorld()
    {
        SKY_ASSERT(light == nullptr);
        auto *lf = GetFeatureProcessor<LightFeatureProcessor>(actor);
        light = lf->GetOrCreateMainLight();
        light->SetColor(data.color);
        light->SetIntensity(data.intensity);
        light->SetDirection(direction);

        binder.Bind(this, actor);
    }

    void MainDirectLightComponent::OnDetachFromWorld()
    {
        SKY_ASSERT(light != nullptr);
        auto *lf = GetFeatureProcessor<LightFeatureProcessor>(actor);
        lf->RemoveMainLight();
        light = nullptr;

        binder.Reset();
    }

    void MainDirectLightComponent::OnTransformChanged(const Transform& global, const Transform& local)
    {
        direction = global.rotation * VEC3_Z;

        if (light != nullptr) {
            light->SetDirection(direction);
        }
    }

} // namespace sky