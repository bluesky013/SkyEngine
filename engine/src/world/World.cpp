//
// Created by Zach Lee on 2021/11/13.
//


#include <world/World.h>
#include <world/GameObject.h>
#include <atomic>

namespace sky {

    GameObject* World::CreateGameObject(const std::string& name)
    {
        static std::atomic_uint32_t index = 0;
        index.fetch_add(1);

        auto go = new GameObject(name);
        go->world = this;
        go->objId = index.load();
        go->AddComponent<TransformComponent>();
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

    void World::SetTarget(Viewport& vp)
    {
    }

    void World::Tick(float time)
    {

    }

    const std::vector<GameObject*>& World::GetGameObjects() const
    {
        return gameObjects;
    }
}