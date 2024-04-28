//
// Created by blues on 2024/5/14.
//

#include <framework/world/Actor.h>

namespace sky {

    ComponentBase *Actor::GetComponent(const Uuid &typeId)
    {
        auto iter = storage.find(typeId);
        return iter != storage.end() ? iter->second.get() : nullptr;
    }

    bool Actor::AddComponent(const Uuid &typeId, ComponentBase* component)
    {
        auto res = storage.emplace(typeId, component);
        return res.second;
    }

    void Actor::RemoveComponent(const Uuid &typeId)
    {
        storage.erase(typeId);
    }

    void Actor::SaveJson(JsonOutputArchive &archive)
    {
        archive.StartObject();
        archive.Key("uuid");
        archive.SaveValue(uuid.ToString());

        archive.Key("components");
        archive.StartArray();
        for (auto &[id, component] : storage) {
            archive.StartObject();
            archive.Key("type");
            archive.SaveValue(id.ToString());
            archive.Key("data");
            component->SaveJson(archive);
            archive.EndObject();
        }
        archive.EndArray();
        archive.EndObject();
    }

    void Actor::LoadJson(JsonInputArchive &archive)
    {
        archive.Start("uuid");
        uuid = Uuid::CreateFromString(archive.LoadString());
        archive.End();

        auto componentCount = archive.StartArray("components");

        auto *context = SerializationContext::Get();

        for (uint32_t i = 0; i < componentCount; ++i) {

            archive.Start("type");
            auto typeId = Uuid::CreateFromString(archive.LoadString());
            archive.End();

            archive.Start("data");
            auto *tmp = static_cast<ComponentBase*>(context->FindTypeById(typeId)->info->newFunc());
            tmp->LoadJson(archive);
            AddComponent(typeId, tmp);
            archive.End();

            archive.NextArrayElement();
        }
        archive.End();
    }

    void Actor::Tick(float time)
    {
        for (auto &[id, component] : storage)
        {
            component->Tick(time);
        }
    }

} // namespace sky