//
// Created by Zach Lee on 2021/11/13.
//


#include <engine/world/World.h>
#include <engine/world/TransformComponent.h>
#include <engine/world/GameObject.h>
#include <atomic>

namespace sky {

    World::World()
        : root(new GameObject("root"))
    {
        root->AddComponent<TransformComponent>();
        gameObjects.emplace_back(root);
    }

    GameObject* World::CreateGameObject(const std::string& name)
    {
        static std::atomic_uint32_t index = 0;
        index.fetch_add(1);

        auto go = new GameObject(name);
        go->world = this;
        go->objId = index.load();
        go->AddComponent<TransformComponent>();
        go->SetParent(root);
        gameObjects.emplace_back(go);
        return go;
    }

    void World::RemoveGameObject(GameObject* go)
    {
        auto iter = std::find(gameObjects.begin(), gameObjects.end(), go);
        if (iter != gameObjects.end()) {
            gameObjects.erase(iter);
        }
    }

    void World::Tick(float time)
    {

    }

    const std::vector<GameObject*>& World::GetGameObjects() const
    {
        return gameObjects;
    }

    GameObject* World::GetRoot()
    {
        return root;
    }

    void World::Reflect()
    {
        TransformComponent::Reflect();
    }
}