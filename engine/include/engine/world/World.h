//
// Created by Zach Lee on 2021/11/12.
//

#pragma once

#include <core/math/Rect.h>
#include <core/std/Container.h>
#include <string>

namespace sky {

    class Viewport;
    class World;
    class GameObject;

    class World {
    public:
        World();
        ~World();

        GameObject *CreateGameObject(const std::string &name);

        void DestroyGameObject(GameObject *);

        void Tick(float);

        const PmrVector<GameObject *> &GetGameObjects() const;

        GameObject *GetRoot();

        static void Reflect();

        template <typename Archive>
        void serialize(Archive &ar)
        {
        }

    private:
        GameObject             *root;
        PmrUnSyncPoolRes        memoryResource;
        PmrVector<GameObject *> gameObjects;
    };
    using WorldPtr = std::shared_ptr<World>;

} // namespace sky
