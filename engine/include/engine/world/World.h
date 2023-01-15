//
// Created by Zach Lee on 2021/11/12.
//

#pragma once

#include <core/util/Uuid.h>
#include <core/math/Rect.h>
#include <core/std/Container.h>
#include <string>
#include <functional>

namespace sky {

    class Viewport;
    class World;
    class GameObject;

    class World {
    public:
        World();
        ~World();

        GameObject *CreateGameObject(const std::string &name);
        GameObject *CreateGameObject(const std::string &name, const Uuid &uuid);

        void DestroyGameObject(GameObject *);

        void Tick(float);

        const PmrVector<GameObject *> &GetGameObjects() const;

        GameObject *GetGameObjectByUuid(const Uuid &id) const;

        GameObject *GetRoot();

        void ForEachBFS(GameObject *go, std::function<void(GameObject *)> && fn) const;

        static void Reflect();

        template <typename Archive>
        void save(Archive &ar) const
        {
            ar(static_cast<uint32_t>(gameObjects.size()));
            ForEachBFS(root, [&ar](GameObject *go) {
                ar(*go);
            });
        }

        template <typename Archive>
        void load(Archive &ar)
        {
            uint32_t size = 0;
            ar(size);
            for (uint32_t i = 0; i < size; ++i) {
                auto go = CreateGameObject("");
                ar(*go);
            }
        }

    private:
        GameObject *AllocateGameObject();
        GameObject                    *root;
        PmrUnSyncPoolRes               memoryResource;
        PmrVector<GameObject *>        gameObjects;
        PmrHashMap<Uuid, GameObject *> objectLut;
    };
    using WorldPtr = std::shared_ptr<World>;

} // namespace sky
