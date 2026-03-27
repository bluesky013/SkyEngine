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
            .Member<&MainLightData::intensity>("Intensity")
            .Member<&MainLightData::castShadow>("CastShadow");

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

        auto *trans = actor->GetComponent<TransformComponent>();
        UpdateData(trans->GetWorldTransform());

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
        UpdateData(global);
    }

    void MainDirectLightComponent::UpdateData(const Transform& global)
    {
        if (light != nullptr) {
            light->SetDirection(global.rotation * VEC3_NZ);
            light->SetWorldMatrix(global.ToMatrix());
        }
    }

    // PointLightComponent
    void PointLightComponent::Reflect(SerializationContext *context)
    {
        context->Register<PointLightData>("PointLightData")
            .Member<&PointLightData::color>("Color")
            .Member<&PointLightData::intensity>("Intensity")
            .Member<&PointLightData::range>("Range");

        REGISTER_BEGIN(PointLightComponent, context)
            REGISTER_MEMBER(LightColor, SetColor, GetColor)
            REGISTER_MEMBER(Intensity, SetIntensity, GetIntensity)
            REGISTER_MEMBER(Range, SetRange, GetRange);
    }

    void PointLightComponent::SetColor(const ColorRGB &color)
    {
        data.color = ColorRGB(color);
        if (light != nullptr) {
            light->SetColor(data.color);
        }
    }

    void PointLightComponent::SetIntensity(float intensity)
    {
        data.intensity = intensity;
        if (light != nullptr) {
            light->SetIntensity(intensity);
        }
    }

    void PointLightComponent::SetRange(float range)
    {
        data.range = range;
        if (light != nullptr) {
            light->SetRange(range);
        }
    }

    void PointLightComponent::OnAttachToWorld()
    {
        SKY_ASSERT(light == nullptr);
        auto *lf = GetFeatureProcessor<LightFeatureProcessor>(actor);
        light = lf->CreateLight<PointLight>();
        light->SetColor(data.color);
        light->SetIntensity(data.intensity);
        light->SetRange(data.range);

        auto *trans = actor->GetComponent<TransformComponent>();
        UpdateData(trans->GetWorldTransform());

        binder.Bind(this, actor);
    }

    void PointLightComponent::OnDetachFromWorld()
    {
        SKY_ASSERT(light != nullptr);
        auto *lf = GetFeatureProcessor<LightFeatureProcessor>(actor);
        lf->RemoveLight(light);
        light = nullptr;

        binder.Reset();
    }

    void PointLightComponent::OnTransformChanged(const Transform& global, const Transform& local)
    {
        UpdateData(global);
    }

    void PointLightComponent::UpdateData(const Transform& global)
    {
        if (light != nullptr) {
            light->SetPosition(global.translation);
        }
    }

    // SpotLightComponent
    void SpotLightComponent::Reflect(SerializationContext *context)
    {
        context->Register<SpotLightComponentData>("SpotLightComponentData")
            .Member<&SpotLightComponentData::color>("Color")
            .Member<&SpotLightComponentData::intensity>("Intensity")
            .Member<&SpotLightComponentData::range>("Range")
            .Member<&SpotLightComponentData::innerAngle>("InnerAngle")
            .Member<&SpotLightComponentData::outerAngle>("OuterAngle");

        REGISTER_BEGIN(SpotLightComponent, context)
            REGISTER_MEMBER(LightColor, SetColor, GetColor)
            REGISTER_MEMBER(Intensity, SetIntensity, GetIntensity)
            REGISTER_MEMBER(Range, SetRange, GetRange)
            REGISTER_MEMBER(InnerAngle, SetInnerAngle, GetInnerAngle)
            REGISTER_MEMBER(OuterAngle, SetOuterAngle, GetOuterAngle);
    }

    void SpotLightComponent::SetColor(const ColorRGB &color)
    {
        data.color = ColorRGB(color);
        if (light != nullptr) {
            light->SetColor(data.color);
        }
    }

    void SpotLightComponent::SetIntensity(float intensity)
    {
        data.intensity = intensity;
        if (light != nullptr) {
            light->SetIntensity(intensity);
        }
    }

    void SpotLightComponent::SetRange(float range)
    {
        data.range = range;
        if (light != nullptr) {
            light->SetRange(range);
        }
    }

    void SpotLightComponent::SetInnerAngle(float angle)
    {
        data.innerAngle = angle;
        if (light != nullptr) {
            light->SetInnerAngle(angle);
        }
    }

    void SpotLightComponent::SetOuterAngle(float angle)
    {
        data.outerAngle = angle;
        if (light != nullptr) {
            light->SetOuterAngle(angle);
        }
    }

    void SpotLightComponent::OnAttachToWorld()
    {
        SKY_ASSERT(light == nullptr);
        auto *lf = GetFeatureProcessor<LightFeatureProcessor>(actor);
        light = lf->CreateLight<SpotLight>();
        light->SetColor(data.color);
        light->SetIntensity(data.intensity);
        light->SetRange(data.range);
        light->SetInnerAngle(data.innerAngle);
        light->SetOuterAngle(data.outerAngle);

        auto *trans = actor->GetComponent<TransformComponent>();
        UpdateData(trans->GetWorldTransform());

        binder.Bind(this, actor);
    }

    void SpotLightComponent::OnDetachFromWorld()
    {
        SKY_ASSERT(light != nullptr);
        auto *lf = GetFeatureProcessor<LightFeatureProcessor>(actor);
        lf->RemoveLight(light);
        light = nullptr;

        binder.Reset();
    }

    void SpotLightComponent::OnTransformChanged(const Transform& global, const Transform& local)
    {
        UpdateData(global);
    }

    void SpotLightComponent::UpdateData(const Transform& global)
    {
        if (light != nullptr) {
            light->SetPosition(global.translation);
            light->SetDirection(global.rotation * VEC3_NZ);
        }
    }

} // namespace sky