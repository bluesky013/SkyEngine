//
// Created by blues on 2026/3/10.
//

#include <shader/shadergraph/ShaderGraphNode.h>
#include <framework/serialization/JsonArchive.h>

namespace sky::sg {

    SGNode::SGNode() : id(Uuid::Create())
    {
    }

    void SGNode::LoadJson(JsonInputArchive& archive)
    {
        archive.LoadKeyValue("id", id);
        archive.LoadKeyValue("name", name);
        archive.LoadKeyValue("posX", posX);
        archive.LoadKeyValue("posY", posY);
    }

    void SGNode::SaveJson(JsonOutputArchive& archive) const
    {
        archive.SaveValueObject("type", GetTypeName());
        archive.SaveValueObject("id", id);
        archive.SaveValueObject("name", name);
        archive.Key("posX"); archive.SaveValue(posX);
        archive.Key("posY"); archive.SaveValue(posY);
    }

} // namespace sky::sg
