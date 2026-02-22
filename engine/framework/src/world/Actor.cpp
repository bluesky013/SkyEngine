//
// Created by blues on 2024/5/14.
//

#include <framework/world/Actor.h>
#include <framework/world/World.h>
#include <framework/world/TransformComponent.h>

namespace sky {

    Actor::~Actor()
    {
        storage.clear();
    }

    ComponentBase *Actor::GetComponent(const Uuid &typeId)
    {
        auto iter = storage.find(typeId);
        return iter != storage.end() ? iter->second.get() : nullptr;
    }

    bool Actor::EmplaceComponent(const Uuid &typeId, ComponentBase* component)
    {
        component->actor = this;
        auto res = storage.emplace(typeId, component);
        if (world != nullptr) {
            component->OnAttachToWorld();
        }
        return res.second;
    }

    ComponentBase *Actor::AddComponent(const Uuid &typeId)
    {
        auto *component = static_cast<ComponentBase*>(SerializationContext::Get()->FindTypeById(typeId)->info->newFunc());
        if (!EmplaceComponent(typeId, component)) {
            delete component;
            component = nullptr;
        }
        return component;
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
        archive.Key("name");
        archive.SaveValue(name);

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

        archive.Start("name");
        name = archive.LoadString();
        archive.End();

        auto componentCount = archive.StartArray("components");

        auto *context = SerializationContext::Get();

        for (uint32_t i = 0; i < componentCount; ++i) {

            archive.Start("type");
            auto typeId = Uuid::CreateFromString(archive.LoadString());
            archive.End();

            archive.Start("data");
            auto* iter = context->FindTypeById(typeId);
            if (iter->info->newFunc != nullptr) {
                auto *tmp = static_cast<ComponentBase*>(context->FindTypeById(typeId)->info->newFunc());
                tmp->LoadJson(archive);
                tmp->actor = this;
                tmp->OnSerialized();
                EmplaceComponent(typeId, tmp);
            }
            archive.End();

            archive.NextArrayElement();
        }
        archive.End();
    }

    void Actor::SetParent(const ActorPtr &parent)
    {
        auto* trans = GetComponent<TransformComponent>();

        Actor* oldActor = nullptr;

        auto* parentTrans = parent ? parent->GetComponent<TransformComponent>() : nullptr;
        if (trans != nullptr) {
            trans->SetParent(parentTrans);
        }

        ActorEvent::BroadCast(this, &IActorEvent::OnParentChanged, oldActor, parent.get());
    }

    void Actor::Tick(float time)
    {
        for (auto &[id, component] : storage) {
            component->Tick(time);
        }
    }

    void Actor::AttachToWorld(World *world_)
    {
        world = world_;
        for (auto &[id, component] : storage) {
            component->OnAttachToWorld();
        }

        ActorEvent::BroadCast(this, &IActorEvent::OnAttachToWorld, world);
    }

    void Actor::DetachFromWorld()
    {
        ActorEvent::BroadCast(this, &IActorEvent::OnDetachFromWorld, world);

        for (auto &[id, component] : storage) {
            component->OnDetachFromWorld();
        }
        world = nullptr;
    }
} // namespace sky