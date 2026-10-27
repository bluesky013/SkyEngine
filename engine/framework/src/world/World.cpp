//
// Created by Zach Lee on 2021/11/13.
//

#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/SimpleRotateComponent.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/World.h>
#include <framework/world/WorldDesc.h>
#include <framework/world/WorldSubSystemRegistry.h>

#include <core/logger/Logger.h>
#include <core/profile/Profiler.h>

#include <memory>
#include <string>
#include <unordered_set>

static const char *TAG = "World";

namespace sky {

    World::~World()
    {
        for (auto &actor : actors) {
            actor->DetachFromWorld();
        }
        actors.clear();
        actorIndex.clear();

        for (auto &sub : subSystems) {
            sub.second->OnDetachFromWorld(*this);
        }
    }

    void World::Reflect(SerializationContext *context)
    {
        TransformComponent::Reflect(context);
        SimpleRotateComponent::Reflect(context);
    }

    World *World::CreateWorld()
    {
        auto *world = new World();
        return world;
    }

    void World::Init()
    {
    }

    const WorldDesc *World::GetWorldDesc() const
    {
        return worldDesc.get();
    }

    WorldDesc *World::GetMutableWorldDesc()
    {
        if (worldDesc == nullptr) {
            worldDesc = std::make_unique<WorldDesc>();
        }
        return worldDesc.get();
    }

    bool World::Build(const WorldDesc &desc)
    {
        lastError.clear();
        const auto fail = [this](std::string reason) {
            lastError = std::move(reason);
            LOG_E(TAG, "%s", lastError.c_str());
            return false;
        };
        if (built) {
            return fail("World is already initialized; apply edited settings to a new runtime world");
        }
        struct PendingSystem {
            std::shared_ptr<const void>      lease;
            std::unique_ptr<IWorldSubSystem> system;
            Name                             name;
        };
        std::vector<PendingSystem> pending;
        std::unordered_set<Name>   names;
        auto                      &registry = WorldSubSystemRegistry::Get();
        for (const auto &entry : desc.subSystems) {
            if (!names.insert(entry.name).second) {
                return fail("Duplicate world subsystem: " + std::string(entry.name.GetStr()));
            }
            if (!entry.enabled) {
                continue;
            }
            if (GetSubSystem(entry.name) != nullptr) {
                return fail("World subsystem is already attached: " + std::string(entry.name.GetStr()));
            }
            const WorldSubSystemRegistration *registration = registry.GetRegistration(entry.name);
            if (registration == nullptr) {
                return fail("World subsystem is unavailable: " + std::string(entry.name.GetStr()));
            }
            if (registration->validate != nullptr) {
                std::string reason;
                if (!registration->validate(entry.config, reason)) {
                    return fail(std::string(entry.name.GetStr()) + ": " + reason);
                }
            }
        }
        for (const auto &actor : actors) {
            for (const auto &[id, component] : actor->GetComponents()) {
                const auto *info = ComponentFactory::Get()->Find(id);
                if (!info) {
                    return fail("Component is not registered: " + id.ToString());
                }
                for (const auto &required : info->requiredSubSystems) {
                    const bool enabled = std::any_of(desc.subSystems.begin(), desc.subSystems.end(),
                                                     [&](const auto &entry) { return entry.name == required && entry.enabled; });
                    if (!enabled && !GetSubSystem(required)) {
                        return fail(info->name + " requires world subsystem " + std::string(required.GetStr()));
                    }
                }
            }
        }
        for (const auto &entry : desc.subSystems) {
            if (!entry.enabled) {
                continue;
            }
            const auto                      *registration = registry.GetRegistration(entry.name);
            auto                             lease        = registration->moduleLease;
            auto                             factory      = registration->factory;
            std::unique_ptr<IWorldSubSystem> subSystem    = factory(*this, entry.config);
            if (!subSystem) {
                return fail("World subsystem creation failed: " + std::string(entry.name.GetStr()));
            }
            pending.push_back({std::move(lease), std::move(subSystem), entry.name});
        }
        if (&desc != worldDesc.get()) {
            worldDesc = std::make_unique<WorldDesc>(desc);
        }
        for (const auto &actor : actors) {
            actor->DetachFromWorld();
        }
        for (auto &entry : pending) {
            AddSubSystem(entry.name, entry.system.release(), std::move(entry.lease));
        }
        for (const auto &actor : actors) {
            std::string error;
            if (!actor->AttachToWorld(this, &error)) {
                for (const auto &attached : actors)
                    attached->DetachFromWorld();
                for (auto it = pending.rbegin(); it != pending.rend(); ++it) {
                    subSystems.at(it->name)->OnDetachFromWorld(*this);
                    subSystems.erase(it->name);
                    subSystemModuleLeases.erase(it->name);
                }
                return fail("Actor " + actor->GetName() + ": " + error);
            }
        }
        built = true;
        return true;
    }

    void World::Tick(float time)
    {
        {
            SKY_PROFILE_NAME("Actors Tick")
            for (auto &actor : actors) {
                actor->Tick(time);
            }
        }

        {
            SKY_PROFILE_NAME("SubSystem Tick")
            for (auto &sys : subSystems) {
                sys.second->Tick(time);
            }
        }
        for (auto &actor : actors)
            actor->PostTick(time);
    }

    void World::SaveJson(JsonOutputArchive &archive)
    {
        archive.StartObject();

        std::vector<ComponentAssetReference> assets;
        for (const auto &actor : actors)
            for (const auto &[type, component] : actor->GetComponents())
                component->CollectAssetReferences(assets);
        archive.Key("assetRoots");
        archive.StartArray();
        for (const auto &asset : assets) {
            archive.StartObject();
            archive.Key("uuid");
            archive.SaveValue(asset.uuid.ToString());
            archive.Key("type");
            archive.SaveValue(asset.type);
            archive.EndObject();
        }
        archive.EndArray();

        archive.Key("actors");
        archive.StartArray();

        for (auto &actor : actors) {
            actor->SaveJson(archive);
        }

        archive.EndArray();

        archive.Key("subSystems");
        archive.StartArray();
        if (worldDesc != nullptr) {
            for (const WorldSubSystemDesc &entry : worldDesc->subSystems) {
                archive.StartObject();
                archive.Key("name");
                archive.SaveValue(entry.name.GetStr());
                archive.Key("enabled");
                archive.SaveValue(entry.enabled);
                if (entry.config) {
                    archive.Key("config");
                    archive.SaveValueObject(entry.config);
                }
                archive.EndObject();
            }
        }
        archive.EndArray();

        archive.EndObject();
    }

    bool World::LoadJson(JsonInputArchive &archive)
    {
        if (!actors.empty() || built) {
            lastError = "Scene loading requires a fresh world";
            return false;
        }
        auto num = archive.StartArray("actors");
        for (uint32_t i = 0; i < num; ++i) {
            auto actor = std::make_unique<Actor>();
            if (!actor->LoadJson(archive, &lastError) || !actor->GetUuid() || actorIndex.contains(actor->GetUuid())) {
                if (lastError.empty()) {
                    lastError = "Invalid or duplicate actor UUID";
                }
                Reset();
                archive.End();
                return false;
            }
            actorIndex.emplace(actor->GetUuid(), actors.size());
            actors.push_back(std::move(actor));
            archive.NextArrayElement();
        }
        archive.End();

        const uint32_t subCount = archive.StartArray("subSystems");
        if (subCount > 0) {
            auto desc = std::make_unique<WorldDesc>();
            for (uint32_t i = 0; i < subCount; ++i) {
                WorldSubSystemDesc entry;
                std::string        name;
                archive.Start("name");
                name = archive.LoadString();
                archive.End();
                entry.name = Name(name.c_str());

                bool enabled = true;
                archive.Start("enabled");
                enabled = archive.LoadBool();
                archive.End();
                entry.enabled = enabled;

                const WorldSubSystemRegistration *registration = WorldSubSystemRegistry::Get().GetRegistration(entry.name);
                if (registration != nullptr && registration->configType != nullptr) {
                    archive.Start("config");
                    entry.config = archive.LoadValueById(registration->configType->registeredId);
                    archive.End();
                }

                desc->subSystems.push_back(std::move(entry));
                archive.NextArrayElement();
            }
            worldDesc = std::move(desc);
        }
        archive.End();

        // resolve hierarchy
        for (auto &actor : actors) {
            auto *trans = actor->GetComponent<TransformComponent>();
            if (trans != nullptr) {
                auto parentActor = GetActorByUuid(trans->GetData().parent);

                if (parentActor != nullptr) {
                    auto *parentTrans = parentActor->GetComponent<TransformComponent>();
                    if (parentTrans != nullptr) {
                        // Preserve the serialized local transform and derive the world transform.
                        trans->SetParentPreserveLocal(parentTrans);
                    }
                }
            }
        }
        return true;
    }

    Actor *World::CreateActor(const char *name, bool withTrans)
    {
        auto *actor = CreateActor(Uuid::Create(), withTrans);
        if (actor)
            actor->SetName(name);
        return actor;
    }

    Actor *World::CreateActor(const std::string &name, bool withTrans)
    {
        auto *actor = CreateActor(Uuid::Create(), withTrans);
        if (actor)
            actor->SetName(name);
        return actor;
    }

    Actor *World::CreateActor(bool withTrans)
    {
        return CreateActor("Actor", withTrans);
    }

    Actor *World::CreateActor(const Uuid &id, bool withTrans)
    {
        auto *actor = AttachToWorld(std::make_unique<Actor>(id));
        if (actor && withTrans) {
            actor->AddComponent<TransformComponent>();
        }
        return actor;
    }

    Actor *World::GetActorByUuid(const Uuid &id)
    {
        auto iter = actorIndex.find(id);
        return iter != actorIndex.end() ? actors[iter->second].get() : nullptr;
    }

    Actor *World::AttachToWorld(std::unique_ptr<Actor> actor)
    {
        return TryAttachToWorld(actor);
    }

    Actor *World::TryAttachToWorld(std::unique_ptr<Actor> &actor)
    {
        lastError.clear();
        if (!actor || actor->GetWorld() || !actor->GetUuid() || actorIndex.contains(actor->GetUuid())) {
            lastError = "Actor attachment requires a unique valid UUID and a detached actor";
            return nullptr;
        }
        Actor *ptr = actor.get();
        actors.emplace_back(std::move(actor));
        actorIndex[ptr->GetUuid()] = actors.size() - 1;
        if (!ptr->AttachToWorld(this, &lastError)) {
            actorIndex.erase(ptr->GetUuid());
            actor = std::move(actors.back());
            actors.pop_back();
            return nullptr;
        }
        WorldEvent::BroadCast(this, &IWorldEvent::OnActorAttached, ptr);
        return ptr;
    }

    std::unique_ptr<Actor> World::DetachFromWorld(Actor *actor)
    {
        auto iter = actorIndex.find(actor->GetUuid());
        if (iter == actorIndex.end()) {
            return nullptr;
        }

        WorldEvent::BroadCast(this, &IWorldEvent::OnActorDetached, actor);
        actor->DetachFromWorld();

        // Swap-remove: O(1) removal, keeping the dense actor list packed.
        const size_t index = iter->second;
        actorIndex.erase(iter);

        std::unique_ptr<Actor> owned = std::move(actors[index]);
        const size_t           last  = actors.size() - 1;
        if (index != last) {
            actors[index]                        = std::move(actors[last]);
            actorIndex[actors[index]->GetUuid()] = index;
        }
        actors.pop_back();
        return owned;
    }

    void World::Reset()
    {
        for (auto &actor : actors) {
            actor->DetachFromWorld();
        }
        actors.clear();
        actorIndex.clear();
    }

    void World::AddSubSystem(const Name &name, IWorldSubSystem *sys, std::shared_ptr<const void> moduleLease)
    {
        SKY_ASSERT(subSystems.emplace(name, sys).second);
        subSystemModuleLeases.emplace(name, std::move(moduleLease));
        sys->OnAttachToWorld(*this);
    }

    IWorldSubSystem *World::GetSubSystem(const Name &name) const
    {
        auto iter = subSystems.find(name);
        return iter != subSystems.end() ? iter->second.get() : nullptr;
    }

    void World::StartSimulation()
    {
        for (auto &sub : subSystems) {
            sub.second->StartSimulation();
        }
    }

    void World::StopSimulation()
    {
        for (auto &sub : subSystems) {
            sub.second->StopSimulation();
        }
    }
} // namespace sky
