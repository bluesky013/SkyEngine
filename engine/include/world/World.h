//
// Created by Zach Lee on 2021/11/12.
//


#pragma once

#include <vector>

namespace sky {

    class GameObject;

    class World {
    public:
        World() = default;
        ~World() = default;

        GameObject* CreateGameObject();
        void RemoveGameObject(GameObject*);

        void Tick();

    private:
        std::vector<GameObject*> gameObjects;
    };

}