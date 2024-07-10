//
// Created by Zach Lee on 2021/11/13.
//

#include <framework/world/World.h>
#include <framework/world/TransformComponent.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/JsonArchive.h>

#include <atomic>
#include <deque>
#include <memory>

namespace sky {

    World::~World()
    {
        WorldEvent::BroadCast(&IWorldEvent::OnDestroyWorld, this);
        actors.clear();
    }

    void World::Reflect(SerializationContext *context)
    {
        TransformComponent::Reflect(context);
    }

    World *World::CreateWorld()
    {
        auto *world = new World();
        if (!world->Init()) {
            delete world;
            world = nullptr;
        }

        return world;
    }

    bool World::Init()
    {
        WorldEvent::BroadCast(&IWorldEvent::OnCreateWorld, this);

        return true;
    }

    void World::Tick(float time)
    {
        for (auto &actor : actors) {
            actor->Tick(time);
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
        actors.emplace_back(std::make_shared<Actor>(id));
        actors.back()->world = this;
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

    void World::AttachToWorld(const sky::ActorPtr &actor)
    {
        if (actor->world != nullptr && actor->world != this) {
            actor->world->DetachFromWorld(actor);
        }
        actors.emplace_back(actor);
        actor->world = this;
    }

    void World::DetachFromWorld(const ActorPtr &actor)
    {
        actor->world = nullptr;
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

    void World::AddSubSystem(const std::string &name, IWorldSubSystem* sys)
    {
        SKY_ASSERT(subSystems.emplace(name, sys).second);
    }

    IWorldSubSystem* World::GetSubSystem(const std::string &name) const
    {
        auto iter = subSystems.find(name);
        return iter != subSystems.end() ? iter->second : nullptr;
    }

} // namespace sky
