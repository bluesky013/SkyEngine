//
// Created by Zach Lee on 2021/11/12.
//


#pragma once

#include <vector>
#include <core/math/Rect.h>

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

        GameObject* CreateGameObject();
        void RemoveGameObject(GameObject*);

        void SetTarget(Viewport& vp);

        void Tick(float);

    private:
        std::vector<GameObject*> gameObjects;
        Viewport* viewport = nullptr;
    };

}