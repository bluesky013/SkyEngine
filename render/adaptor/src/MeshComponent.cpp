//
// Created by Zach Lee on 2023/2/28.
//

#include <render/adaptor/MeshComponent.h>
#include <framework/serialization/JsonArchive.h>

namespace sky {

    void MeshComponent::Reflect()
    {
        SerializationContext::Get()
            ->Register<MeshComponent>(NAME);

        ComponentFactory::Get()->RegisterComponent<MeshComponent>();
    }

    void MeshComponent::Save(JsonOutputArchive &ar) const
    {
        ar.StartObject();
        ar.EndObject();
    }

    void MeshComponent::Load(JsonInputArchive &ar)
    {

    }

} // namespace sky