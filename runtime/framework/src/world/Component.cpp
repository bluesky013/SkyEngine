//
// Created by blues on 2025/7/7.
//

#include <framework/world/Component.h>

namespace sky {

    void ComponentBase::SaveJson(JsonOutputArchive &archive) const
    {
        archive.StartObject();
        archive.EndObject();
    }

    void ComponentBase::LoadJson(JsonInputArchive &archive)
    {

    }

} // namespace sky