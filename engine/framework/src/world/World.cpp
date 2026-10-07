//
// Created by Zach Lee on 2021/11/13.
//

#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/SimpleRotateComponent.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/World.h>
#include <framework/world/WorldDesc.h>
#include <framework/world/WorldSubSystemRegistry.h>

#include <core/logger/Logger.h>
#include <core/profile/Profiler.h>

#include <memory>
#include <string>

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

    void World::Build(const WorldDesc &desc)
    {
        worldDesc      = std::make_unique<WorldDesc>(desc);
        auto &registry = WorldSubSystemRegistry::Get();
        for (const auto &entry : worldDesc->subSystems) {
            if (!entry.enabled) {
                continue;
            }
            if (GetSubSystem(entry.name) != nullptr) {
                LOG_W(TAG, "world subsystem already present, skipping");
                continue;
            }
            const WorldSubSystemRegistration *registration = registry.GetRegistration(entry.name);
            if (registration == nullptr) {
                LOG_W(TAG, "world subsystem '%.*s' not registered, skipping", static_cast<int>(entry.name.GetStr().size()),
                      entry.name.GetStr().data());
                continue;
            }
            if (registration->validate != nullptr) {
                std::string reason;
                if (!registration->validate(entry.config, reason)) {
                    LOG_E(TAG, "world subsystem '%.*s' config invalid: %s", static_cast<int>(entry.name.GetStr().size()), entry.name.GetStr().data(),
                          reason.c_str());
                    SKY_ASSERT(false); // develop/debug: fail loudly; release: skip and continue
                    continue;
                }
            }
            std::unique_ptr<IWorldSubSystem> subSystem = registration->factory(*this, entry.config);
            if (subSystem != nullptr) {
                AddSubSystem(entry.name, subSystem.release());
            }
        }
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
    }

    void World::SaveJson(JsonOutputArchive &archive)
    {
        archive.StartObject();

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

    void World::LoadJson(JsonInputArchive &archive)
    {
        auto num = archive.StartArray("actors");
        for (uint32_t i = 0; i < num; ++i) {
            auto actor = std::make_unique<Actor>();
            actor->LoadJson(archive);
            AttachToWorld(std::move(actor));
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
    }

    Actor *World::CreateActor(const char *name, bool withTrans)
    {
        auto *actor = CreateActor(Uuid::Create(), withTrans);
        actor->SetName(name);
        return actor;
    }

    Actor *World::CreateActor(const std::string &name, bool withTrans)
    {
        auto *actor = CreateActor(Uuid::Create(), withTrans);
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
        if (withTrans) {
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
        SKY_ASSERT(actor != nullptr);
        SKY_ASSERT(actor->world == nullptr);

        Actor *ptr = actor.get();
        actors.emplace_back(std::move(actor));
        actorIndex[ptr->GetUuid()] = actors.size() - 1;
        ptr->AttachToWorld(this);

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

    void World::AddSubSystem(const Name &name, IWorldSubSystem *sys)
    {
        SKY_ASSERT(subSystems.emplace(name, sys).second);
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
