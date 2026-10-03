//
// Created by Zach Lee on 2021/11/13.
//

#include <framework/world/World.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/SimpleRotateComponent.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/JsonArchive.h>

#include <core/profile/Profiler.h>

#include <memory>

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
        const size_t last = actors.size() - 1;
        if (index != last) {
            actors[index] = std::move(actors[last]);
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

    void World::AddSubSystem(const Name &name, IWorldSubSystem* sys)
    {
        SKY_ASSERT(subSystems.emplace(name, sys).second);
        sys->OnAttachToWorld(*this);
    }

    IWorldSubSystem* World::GetSubSystem(const Name &name) const
    {
        auto iter = subSystems.find(name);
        return iter != subSystems.end() ? iter->second.get() : nullptr;
    }

    void World::RegisterConfiguration(const Name& name, const Any& any)
    {
        SKY_ASSERT(worldConfigs.emplace(name, any).second);
    }

    const Any& World::GetConfigByName(const Name &name) const
    {
        static Any EMPTY;
        auto iter = worldConfigs.find(name);
        return iter != worldConfigs.end() ? iter->second : EMPTY;
    }
} // namespace sky
