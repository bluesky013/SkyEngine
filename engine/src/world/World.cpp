//
// Created by Zach Lee on 2021/11/13.
//

#include <atomic>
#include <engine/base/GameObject.h>
#include <engine/world/TransformComponent.h>
#include <engine/world/World.h>

namespace sky {

    World::World() : root(nullptr), gameObjects(&memoryResource)
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

    GameObject *World::CreateGameObject(const std::string &name)
    {
        static std::atomic_uint32_t index = 0;
        index.fetch_add(1);
        auto ptr = memoryResource.allocate(sizeof(GameObject));
        auto go = new (ptr) GameObject(name);
        go->resource = &memoryResource;
        go->world = this;
        go->objId = index.load();
        go->AddComponent<TransformComponent>();
        go->SetParent(root);
        gameObjects.emplace_back(go);
        return go;
    }

    void World::DestroyGameObject(GameObject *go)
    {
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

    GameObject *World::GetRoot()
    {
        return root;
    }

    void World::Reflect()
    {
        TransformComponent::Reflect();
    }
} // namespace sky
