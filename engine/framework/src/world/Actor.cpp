//
// Created by blues on 2024/5/14.
//

#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/World.h>

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

    bool Actor::EmplaceComponent(const Uuid &typeId, ComponentBase *component)
    {
        component->actor = this;
        auto res         = storage.emplace(typeId, component);
        if (!res.second) {
            return false;
        }

        if (world != nullptr) {
            component->OnAttachToWorld();
        }
        return true;
    }

    ComponentBase *Actor::AddComponent(const Uuid &typeId)
    {
        auto *node = SerializationContext::Get()->FindTypeById(typeId);
        if (node == nullptr || node->info == nullptr || node->info->newFunc == nullptr) {
            // Unknown or non-constructible component type: skip without crashing.
            return nullptr;
        }

        auto *component = static_cast<ComponentBase *>(node->info->newFunc());
        if (!EmplaceComponent(typeId, component)) {
            delete component;
            component = nullptr;
        }
        return component;
    }

    void Actor::RemoveComponent(const Uuid &typeId)
    {
        auto iter = storage.find(typeId);
        if (iter == storage.end()) {
            return;
        }

        if (world != nullptr) {
            iter->second->OnDetachFromWorld();
        }
        storage.erase(iter);
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
        for (const auto &[id, component] : storage) {
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
            auto *node = context->FindTypeById(typeId);
            if (node != nullptr && node->info != nullptr && node->info->newFunc != nullptr) {
                auto *tmp = static_cast<ComponentBase *>(node->info->newFunc());
                tmp->LoadJson(archive);
                tmp->actor = this;
                tmp->OnSerialized();
                if (!EmplaceComponent(typeId, tmp)) {
                    delete tmp;
                }
            }
            archive.End();

            archive.NextArrayElement();
        }
        archive.End();
    }

    void Actor::SetParent(Actor *parent)
    {
        auto *trans = GetComponent<TransformComponent>();

        Actor *oldActor = nullptr;
        if (trans != nullptr) {
            auto *oldParentTrans = trans->GetParent();
            oldActor             = oldParentTrans != nullptr ? oldParentTrans->GetActor() : nullptr;
        }

        auto *parentTrans = parent != nullptr ? parent->GetComponent<TransformComponent>() : nullptr;
        if (trans != nullptr) {
            trans->SetParent(parentTrans);
        }

        ActorEvent::BroadCast(this, &IActorEvent::OnParentChanged, oldActor, parent);
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