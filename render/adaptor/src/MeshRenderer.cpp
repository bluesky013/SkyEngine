//
// Created by Zach Lee on 2023/2/28.
//

#include <framework/serialization/JsonArchive.h>
#include <render/adaptor/MeshRenderer.h>

namespace sky {

    void MeshRenderer::Reflect()
    {
        SerializationContext::Get()
            ->Register<MeshRenderer>(NAME);

        ComponentFactory::Get()->RegisterComponent<MeshRenderer>();
    }

    void MeshRenderer::Save(JsonOutputArchive &ar) const
    {
        ar.StartObject();
        ar.EndObject();
    }

    void MeshRenderer::Load(JsonInputArchive &ar)
    {

    }

} // namespace sky