//
// Created by Zach Lee on 2021/11/13.
//

#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/World.h>

#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/JsonArchive.h>

#include <atomic>
#include <deque>

namespace sky {

    void World::Reflect()
    {
        SerializationContext::Get()->Register<World>("World")
            .JsonLoad<&World::Load>()
            .JsonSave<&World::Save>();

        SerializationContext::Get()->Register<Component>("Component")
            .JsonLoad<&Component::Load>()
            .JsonSave<&Component::Save>();

        Actor::Reflect();
        TransformComponent::Reflect();
    }

    World::World() : root(nullptr), renderScene(nullptr), actors(&memoryResource), objectLut(&memoryResource)
    {
        root = CreateActor("root");
    }

    World::~World()
    {
        for (auto &go : actors) {
            go->~Actor();
            memoryResource.deallocate(go, sizeof(Actor));
        }
        actors.clear();
    }

    Actor *World::AllocateGameObject()
    {
        static std::atomic_uint32_t index = 0;
        index.fetch_add(1);
        auto ptr = memoryResource.allocate(sizeof(Actor));
        auto go = new (ptr) Actor();
        go->resource = &memoryResource;
        go->world = this;
        go->objId = index.load();
        return go;
    }

    Actor *World::CreateActor(const std::string &name, const Uuid &uuid)
    {
        auto go = AllocateGameObject();
        go->AddComponent<TransformComponent>();
        go->SetParent(root);
        go->SetUuid(uuid);
        go->SetName(name);
        actors.emplace_back(go);
        objectLut.emplace(uuid, go);
        return go;
    }

    Actor *World::CreateActor(const std::string &name)
    {
        return CreateActor(name, Uuid::Create());
    }

    void World::DestroyActor(Actor *actor)
    {
        if (actor == root) {
            return;
        }

        auto iter = std::find(actors.begin(), actors.end(), actor);
        if (iter != actors.end()) {
            (*iter)->~Actor();
            actors.erase(iter);
            memoryResource.deallocate((*iter), sizeof(Actor));
        }
    }

    void World::Tick(float time)
    {
        for (auto &obj : actors) {
            obj->Tick(time);
        }
    }

    const PmrVector<Actor *> &World::GetActors() const
    {
        return actors;
    }

    Actor *World::GetActorByUuid(const Uuid &id) const
    {
        auto iter = objectLut.find(id);
        return iter != objectLut.end() ? iter->second : nullptr;
    }

    Actor *World::GetRoot()
    {
        return root;
    }

    void World::ForEachBFS(Actor *go, std::function<void(Actor *)> && fn) const
    {
        std::deque<Actor*> queue;
        queue.emplace_back(go);

        while (!queue.empty()) {
            auto tgo = queue.front();
            queue.pop_front();
            if (tgo != root) {
                fn(tgo);
            }

            auto trans = tgo->GetComponent<TransformComponent>();
            auto &children = trans->GetChildren();
            for (auto &child : children) {
                queue.emplace_back(child->object);
            }
        }
    }

    void World::Save(JsonOutputArchive &ar) const
    {
        ar.StartObject();
        ar.Key("objects");
        ar.StartArray();
        ForEachBFS(root, [&ar](Actor *go) {
            ar.SaveValueObject(*go);
        });
        ar.EndArray();
        ar.EndObject();
    }

    void World::Load(JsonInputArchive &ar)
    {
        uint32_t size = ar.StartArray("objects");
        for (uint32_t i = 0; i < size; ++i) {
            auto go = CreateActor("");
            ar.LoadArrayElement(*go);
        }
        ar.End();
    }
} // namespace sky
