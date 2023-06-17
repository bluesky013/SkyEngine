//
// Created by Zach Lee on 2023/2/28.
//

#include <render/adaptor/LightComponent.h>
#include <framework/serialization/JsonArchive.h>

namespace sky {

    void LightComponent::Reflect()
    {
        SerializationContext::Get()
            ->Register<LightComponent>(NAME);

        ComponentFactory::Get()->RegisterComponent<LightComponent>();
    }

    void LightComponent::Save(JsonOutputArchive &ar) const
    {
        ar.StartObject();
        ar.EndObject();
    }

    void LightComponent::Load(JsonInputArchive &ar)
    {

    }


} // namespace sky