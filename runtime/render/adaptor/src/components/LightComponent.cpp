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

    void LightComponent::Reflect(SerializationContext *context)
    {
        context->Register<LightComponent>("LightComponent")
            .Member<&LightComponent::lightColor>("lightColor");
    }

    void LightComponent::Tick(float time)
    {
        if (light == nullptr) {
            return;
        }

        auto *transform = actor->GetComponent<TransformComponent>();
        auto world = Cast(transform->GetWorldMatrix());
        auto direct = world * (-VEC3_Z);
        light->SetDirection(direct);
        light->SetColor(Cast(lightColor));
    }

    void LightComponent::OnActive()
    {
        SKY_ASSERT(light == nullptr);
        auto *lf = GetFeatureProcessor<LightFeatureProcessor>(actor);
        light = lf->CreateLight<DirectLight>();
    }

    void LightComponent::OnDeActive()
    {
        SKY_ASSERT(light != nullptr);
        auto *lf = GetFeatureProcessor<LightFeatureProcessor>(actor);
        lf->RemoveLight(light);
    }

    void LightComponent::SaveJson(JsonOutputArchive &ar) const
    {
        ar.StartObject();
        ar.SaveValueObject(std::string("lightColor"), lightColor);
        ar.EndObject();
    }

    void LightComponent::LoadJson(JsonInputArchive &ar)
    {
        ar.LoadKeyValue("lightColor", lightColor);
    }


} // namespace sky