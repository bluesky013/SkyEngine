//
// Created by Zach Lee on 2023/2/28.
//

#include <render/adaptor/components/LightComponent.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/PropertyCommon.h>

namespace sky {

    void LightComponent::Reflect()
    {
        SerializationContext::Get()
            ->Register<LightComponent>(NAME)
            .Member<&LightComponent::lightColor>("lightColor");

        ComponentFactory::Get()->RegisterComponent<LightComponent>();
    }

    void LightComponent::Save(JsonOutputArchive &ar) const
    {
        ar.StartObject();
        ar.SaveValueObject(std::string("lightColor"), lightColor);
        ar.EndObject();
    }

    void LightComponent::Load(JsonInputArchive &ar)
    {
        ar.LoadKeyValue("lightColor", lightColor);
    }


} // namespace sky