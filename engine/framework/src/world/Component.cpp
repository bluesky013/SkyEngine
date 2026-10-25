//
// Created by blues on 2025/7/7.
//

#include <framework/world/Component.h>

namespace sky {

    void ComponentBase::CollectAssetReferences(std::vector<ComponentAssetReference> &out) const
    {
        const auto *node   = GetTypeNode(GetTypeId());
        auto       *object = const_cast<ComponentBase *>(this)->GetReflectedObject();
        if (!node || !object)
            return;
        for (const auto &[name, member] : node->members) {
            const auto kind = member.properties.find(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE));
            if (kind == member.properties.end())
                continue;
            std::string type;
            const auto *kindInfo = kind->second.Info();
            if (kindInfo && kindInfo->registeredId == TypeInfo<std::string>::RegisteredId())
                type = *kind->second.GetAsConst<std::string>();
            else if (kindInfo && kindInfo->registeredId == TypeInfo<std::string_view>::RegisteredId())
                type = *kind->second.GetAsConst<std::string_view>();
            const auto  value = member.getterConstFn ? member.getterConstFn(object) : member.getterFn ? member.getterFn(object) : Any{};
            const auto *id    = value.Info() && value.Info()->registeredId == TypeInfo<Uuid>::RegisteredId() ? value.GetAsConst<Uuid>() : nullptr;
            if (!type.empty() && id && *id)
                out.push_back({*id, std::move(type)});
        }
    }

    void ComponentBase::SaveJson(JsonOutputArchive &archive) const
    {
        archive.StartObject();
        archive.EndObject();
    }

    void ComponentBase::LoadJson(JsonInputArchive &archive)
    {
    }

} // namespace sky
