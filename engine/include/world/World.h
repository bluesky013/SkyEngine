//
// Created by Zach Lee on 2021/11/12.
//


#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <core/math/Rect.h>
#include <world/TransformComponent.h>

namespace sky {

    class GameObject;
    class Viewport;
    class World;

    struct IWorldEvent {
        virtual void OnViewportChange(Viewport& vp) {}
    };

    class World {
    public:
        World() = default;
        ~World() = default;

        GameObject* CreateGameObject(const std::string& name);

        void RemoveGameObject(GameObject*);

        void SetTarget(Viewport& vp);

        void Tick(float);

        const std::vector<GameObject*>& GetGameObjects() const;

    private:
        std::vector<GameObject*> gameObjects;
        Viewport* viewport = nullptr;
    };

}