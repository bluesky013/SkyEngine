//
// Created by Zach Lee on 2021/11/13.
//


#include <world/World.h>
#include <world/GameObject.h>

namespace sky {

    GameObject* World::CreateGameObject()
    {
        auto go = new GameObject();
        go->world = this;
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
        EachListener([&vp](IWorldEvent* event) {
            event->OnViewportChange(vp);
        });
    }

    void World::Tick(float time)
    {

    }

    void World::RegisterWorldListener(IWorldEvent *event)
    {
        if (event != nullptr) {
            eventListeners.emplace_back(event);
        }
    }

    void World::UnRegisterWorldListener(IWorldEvent *event)
    {
        auto iter = std::find(eventListeners.begin(), eventListeners.end(), event);
        if (iter != eventListeners.end()) {
            eventListeners.erase(iter);
        }
    }
}