//
// Created by Zach Lee on 2021/11/13.
//

#include <framework/world/World.h>
#include <framework/world/TransformComponent.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/JsonArchive.h>

#include <atomic>
#include <deque>

namespace sky {

    World::~World()
    {
        WorldEvent::BroadCast(&IWorldEvent::OnDestroyWorld, *this);
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
        WorldEvent::BroadCast(&IWorldEvent::OnCreateWorld, *this);
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
            auto *actor = CreateActor();
            actor->LoadJson(archive);
            archive.NextArrayElement();
        }
        archive.End();
    }

    void World::SaveBinary(BinaryOutputArchive &archive)
    {
        
    }

    void World::LoadBinary(BinaryInputArchive &archive)
    {

    }

    Actor* World::CreateActor(const std::string &name)
    {
        auto *actor = CreateActor(Uuid{});
        actor->AddComponent<TransformComponent>();
        actor->SetName(name);
        return actor;
    }

    Actor* World::CreateActor()
    {
        return CreateActor("Actor");
    }

    Actor* World::CreateActor(const Uuid &id)
    {
        actors.emplace_back(new Actor(id));
        actors.back()->world = this;
        return actors.back().get();
    }

    Actor* World::GetActorByUuid(const Uuid &id)
    {
        auto iter = std::find_if(actors.begin(), actors.end(), [&id](const auto &v) {
            return id == v->GetUuid();
        });
        return iter != actors.end() ? iter->get() : nullptr;
    }

    void World::DestroyActor(Actor *actor)
    {
        auto iter = std::find_if(actors.begin(), actors.end(),
            [actor](const auto &v) { return actor == v.get(); });

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
