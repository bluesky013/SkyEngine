//
// Created by Zach Lee on 2021/11/13.
//

#include <framework/world/World.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/SimpleRotateComponent.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/JsonArchive.h>

#include <core/profile/Profiler.h>

#include <atomic>
#include <deque>
#include <memory>

namespace sky {

    World::~World()
    {
        for (auto &actor : actors) {
            actor->DetachFromWorld();
        }
        actors.clear();

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
            auto actor = std::make_shared<Actor>();
            actor->LoadJson(archive);
            AttachToWorld(actor);
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
                    SKY_ASSERT(parentTrans);
                    trans->SetParent(parentTrans);
                }
            }
        }
    }

    ActorPtr World::CreateActor(const char *name, bool withTrans)
    {
        auto actor = CreateActor(Uuid::Create(), withTrans);
        actor->SetName(name);
        return actor;
    }

    ActorPtr World::CreateActor(const std::string &name, bool withTrans)
    {
        auto actor = CreateActor(Uuid::Create(), withTrans);
        actor->SetName(name);
        return actor;
    }

    ActorPtr World::CreateActor(bool withTrans)
    {
        return CreateActor("Actor", withTrans);
    }

    ActorPtr World::CreateActor(const Uuid &id, bool withTrans)
    {
        AttachToWorld(std::make_shared<Actor>(id));
        if (withTrans) {
            actors.back()->AddComponent<TransformComponent>();
        }
        return actors.back();
    }

    ActorPtr World::GetActorByUuid(const Uuid &id)
    {
        auto iter = std::find_if(actors.begin(), actors.end(), [&id](const auto &v) {
            return id == v->GetUuid();
        });
        return iter != actors.end() ? *iter : ActorPtr{};
    }

    void World::AttachToWorld(const ActorPtr &actor)
    {
        if (actor->world != nullptr && actor->world != this) {
            actor->world->DetachFromWorld(actor);
        }
        actors.emplace_back(actor);
        actor->AttachToWorld(this);

        WorldEvent::BroadCast(this, &IWorldEvent::OnActorAttached, actor);
    }

    void World::DetachFromWorld(const ActorPtr &actor)
    {
        WorldEvent::BroadCast(this, &IWorldEvent::OnActorDetached, actor);

        actor->DetachFromWorld();
        auto iter = std::find_if(actors.begin(), actors.end(),
            [&actor](const auto &v) { return actor == v; });

        if (iter != actors.end()) {
            actors.erase(iter);
        }
    }

    void World::Reset()
    {
        actors.clear();
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
