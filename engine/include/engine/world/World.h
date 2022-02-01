//
// Created by Zach Lee on 2021/11/12.
//


#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <core/math/Rect.h>

namespace sky {

    class Viewport;
    class World;
    class GameObject;

    class World {
    public:
        World();
        ~World();

        GameObject* CreateGameObject(const std::string& name);

        void RemoveGameObject(GameObject*);

        void Tick(float);

        const std::vector<GameObject*>& GetGameObjects() const;

        GameObject* GetRoot();

        static void Reflect();

    private:
        GameObject* root;
        std::vector<GameObject*> gameObjects;
    };

}