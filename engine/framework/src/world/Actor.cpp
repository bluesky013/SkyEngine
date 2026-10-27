//
// Created by blues on 2024/5/14.
//

#include <framework/world/Actor.h>
#include <framework/world/ComponentFactory.h>
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
        if (!component || component->GetTypeId() != typeId || storage.contains(typeId))
            return false;
        component->actor = this;
        auto res         = storage.emplace(typeId, component);
        if (!res.second) {
            return false;
        }

        lastError.clear();
        if (world != nullptr && !component->TryAttachToWorld(lastError)) {
            component->OnDetachFromWorld();
            component->actor = nullptr;
            res.first->second.release();
            storage.erase(res.first);
            return false;
        }
        return true;
    }

    ComponentBase *Actor::AddComponent(const Uuid &typeId)
    {
        auto owned = ComponentFactory::Get()->Create(typeId);
        if (!owned) {
            return nullptr;
        }
        auto *component = owned.release();
        if (!EmplaceComponent(typeId, component)) {
            delete component;
            component = nullptr;
        }
        return component;
    }

    void Actor::RemoveComponent(const Uuid &typeId)
    {
        DetachComponent(typeId);
    }

    Actor::ComponentPtr Actor::DetachComponent(const Uuid &typeId)
    {
        auto iter = storage.find(typeId);
        if (iter == storage.end()) {
            return {};
        }

        if (world != nullptr) {
            iter->second->OnDetachFromWorld();
        }
        auto component = std::move(iter->second);
        storage.erase(iter);
        component->actor = nullptr;
        return component;
    }

    bool Actor::AttachComponent(ComponentPtr &component)
    {
        if (!component || component->GetActor() || storage.contains(component->GetTypeId()))
            return false;
        auto *raw = component.get();
        storage.emplace(raw->GetTypeId(), std::move(component));
        raw->actor = this;
        lastError.clear();
        if (world && !raw->TryAttachToWorld(lastError)) {
            raw->OnDetachFromWorld();
            raw->actor = nullptr;
            auto found = storage.find(raw->GetTypeId());
            component  = std::move(found->second);
            storage.erase(found);
            return false;
        }
        return true;
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

    bool Actor::LoadJson(JsonInputArchive &archive, std::string *error)
    {
        if (world || !storage.empty()) {
            if (error)
                *error = "Actor loading requires a fresh detached actor";
            return false;
        }
        archive.Start("uuid");
        uuid = Uuid::CreateFromString(archive.LoadString());
        archive.End();

        archive.Start("name");
        name = archive.LoadString();
        archive.End();

        auto componentCount = archive.StartArray("components");

        for (uint32_t i = 0; i < componentCount; ++i) {

            archive.Start("type");
            auto typeId = Uuid::CreateFromString(archive.LoadString());
            archive.End();

            archive.Start("data");
            auto component = ComponentFactory::Get()->Create(typeId);
            if (component && !storage.contains(typeId)) {
                auto *tmp = component.release();
                tmp->LoadJson(archive);
                tmp->actor = this;
                tmp->OnSerialized();
                if (!EmplaceComponent(typeId, tmp)) {
                    delete tmp;
                }
            } else {
                if (error) {
                    *error = "Unknown or duplicate component type: " + typeId.ToString();
                }
                archive.End();
                archive.End();
                return false;
            }
            archive.End();

            archive.NextArrayElement();
        }
        archive.End();
        return true;
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

    bool Actor::AttachToWorld(World *world_, std::string *error)
    {
        if (!world_ || world) {
            lastError = "Actor attachment requires a destination and a detached actor";
            if (error)
                *error = lastError;
            return false;
        }
        lastError.clear();
        world = world_;
        std::vector<ComponentBase *> attached;
        for (auto &[id, component] : storage) {
            if (!component->TryAttachToWorld(lastError)) {
                component->OnDetachFromWorld();
                for (auto it = attached.rbegin(); it != attached.rend(); ++it)
                    (*it)->OnDetachFromWorld();
                world = nullptr;
                if (error)
                    *error = lastError;
                return false;
            }
            attached.push_back(component.get());
        }
        ActorEvent::BroadCast(this, &IActorEvent::OnAttachToWorld, world);
        return true;
    }

    void Actor::PostTick(float time)
    {
        for (auto &[id, component] : storage)
            component->PostTick(time);
    }

    void Actor::DetachFromWorld()
    {
        if (world == nullptr) {
            return;
        }
        ActorEvent::BroadCast(this, &IActorEvent::OnDetachFromWorld, world);

        for (auto &[id, component] : storage) {
            component->OnDetachFromWorld();
        }
        world = nullptr;
    }
} // namespace sky
