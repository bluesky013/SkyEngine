//
// Created by Zach Lee on 2023/2/28.
//

#include <render/adaptor/components/LightComponent.h>
#include <framework/serialization/JsonArchive.h>

namespace sky {

    void LightComponent::Reflect(SerializationContext *context)
    {
        context->Register<LightComponent>("LightComponent")
            .Member<&LightComponent::lightColor>("lightColor");
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