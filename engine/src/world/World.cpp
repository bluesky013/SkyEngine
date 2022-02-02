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
        serviceManager = std::make_unique<ServiceManager>();

        root->AddComponent<TransformComponent>();
        root->world = this;
        gameObjects.emplace_back(root);
    }

    World::~World()
    {
        for (auto& go : gameObjects) {
            delete go;
        }
        gameObjects.clear();
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

    void World::DestroyGameObject(GameObject *go)
    {
        auto iter = std::find(gameObjects.begin(), gameObjects.end(), go);
        if (iter != gameObjects.end()) {
            delete (*iter);
            gameObjects.erase(iter);
        }
    }

    void World::Tick(float time)
    {
        for (auto& obj : gameObjects) {
            obj->Tick(time);
        }
        if (serviceManager) {
            serviceManager->Tick(time);
        }
    }

    const std::vector<GameObject*>& World::GetGameObjects() const
    {
        return gameObjects;
    }

    GameObject* World::GetRoot()
    {
        return root;
    }

    ServiceManager* World::GetServiceManager() const
    {
        return serviceManager.get();
    }

    void World::Reflect()
    {
        TransformComponent::Reflect();
    }
}