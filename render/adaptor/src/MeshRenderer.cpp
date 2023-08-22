//
// Created by Zach Lee on 2023/2/28.
//

#include <framework/serialization/JsonArchive.h>
#include <render/adaptor/MeshRenderer.h>

namespace sky {

    void MeshRenderer::Reflect()
    {
        SerializationContext::Get()
            ->Register<MeshRenderer>(NAME)
            .Member<&MeshRenderer::isStatic>("static")
            .Member<&MeshRenderer::castShadow>("castShadow")
            .Member<&MeshRenderer::receiveShadow>("receiveShadow");

        ComponentFactory::Get()->RegisterComponent<MeshRenderer>();
    }

    void MeshRenderer::Save(JsonOutputArchive &ar) const
    {
        ar.StartObject();
        ar.SaveValueObject(std::string("static"), isStatic);
        ar.SaveValueObject(std::string("castShadow"), castShadow);
        ar.SaveValueObject(std::string("receiveShadow"), receiveShadow);
        ar.EndObject();
    }

    void MeshRenderer::Load(JsonInputArchive &ar)
    {
        ar.LoadKeyValue("static", isStatic);
        ar.LoadKeyValue("castShadow", castShadow);
        ar.LoadKeyValue("receiveShadow", receiveShadow);
    }

} // namespace sky