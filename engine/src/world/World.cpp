//
// Created by Zach Lee on 2021/11/13.
//

#include <engine/base/GameObject.h>
#include <engine/world/TransformComponent.h>
#include <engine/world/CameraComponent.h>
#include <engine/world/World.h>
#include <atomic>
#include <deque>

namespace sky {

    World::World() : root(nullptr), gameObjects(&memoryResource), objectLut(&memoryResource)
    {
        root = CreateGameObject("root");
    }

    World::~World()
    {
        for (auto &go : gameObjects) {
            go->~GameObject();
            memoryResource.deallocate(go, sizeof(GameObject));
        }
        gameObjects.clear();
    }

    GameObject *World::AllocateGameObject()
    {
        static std::atomic_uint32_t index = 0;
        index.fetch_add(1);
        auto ptr = memoryResource.allocate(sizeof(GameObject));
        auto go = new (ptr) GameObject();
        go->resource = &memoryResource;
        go->world = this;
        go->objId = index.load();
        return go;
    }

    GameObject *World::CreateGameObject(const std::string &name, const Uuid &uuid)
    {
        auto go = AllocateGameObject();
        go->AddComponent<TransformComponent>();
        go->SetParent(root);
        go->SetUuid(uuid);
        go->SetName(name);
        gameObjects.emplace_back(go);
        objectLut.emplace(uuid, go);
        return go;
    }

    GameObject *World::CreateGameObject(const std::string &name)
    {
        return CreateGameObject(name, Uuid::Create());
    }

    void World::DestroyGameObject(GameObject *go)
    {
        if (go == root) {
            return;
        }

        auto iter = std::find(gameObjects.begin(), gameObjects.end(), go);
        if (iter != gameObjects.end()) {
            (*iter)->~GameObject();
            gameObjects.erase(iter);
            memoryResource.deallocate((*iter), sizeof(GameObject));
        }
    }

    void World::Tick(float time)
    {
        for (auto &obj : gameObjects) {
            obj->Tick(time);
        }
    }

    const PmrVector<GameObject *> &World::GetGameObjects() const
    {
        return gameObjects;
    }

    GameObject *World::GetGameObjectByUuid(const Uuid &id) const
    {
        auto iter = objectLut.find(id);
        return iter != objectLut.end() ? iter->second : nullptr;
    }

    GameObject *World::GetRoot()
    {
        return root;
    }

    void World::ForEachBFS(GameObject *go, std::function<void(GameObject *)> && fn) const
    {
        std::deque<GameObject*> queue;
        queue.emplace_back(go);

        while (!queue.empty()) {
            auto tgo = queue.front();
            queue.pop_front();
            fn(tgo);

            auto trans = tgo->GetComponent<TransformComponent>();
            auto &children = trans->GetChildren();
            for (auto &child : children) {
                queue.emplace_back(child->object);
            }
        }

    }

    void World::Reflect()
    {
        TransformComponent::Reflect();
        CameraComponent::Reflect();
    }
} // namespace sky
