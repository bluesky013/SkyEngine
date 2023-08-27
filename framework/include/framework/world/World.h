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

    class World;
    class IRenderScene;
    class GameObject;

    class JsonOutputArchive;
    class JsonInputArchive;

    class World {
    public:
        World();
        ~World();

        World(const World&) = delete;
        World &operator=(const World&) = delete;

        static void Reflect();

        void SetRenderScene(IRenderScene *scn) { renderScene = scn; }
        IRenderScene *GetRenderScene() const { return renderScene; }

        GameObject *CreateGameObject(const std::string &name);
        GameObject *CreateGameObject(const std::string &name, const Uuid &uuid);

        void DestroyGameObject(GameObject *);

        void Tick(float);

        const PmrVector<GameObject *> &GetGameObjects() const;

        GameObject *GetGameObjectByUuid(const Uuid &id) const;

        GameObject *GetRoot();

        void ForEachBFS(GameObject *go, std::function<void(GameObject *)> && fn) const;

        void Save(JsonOutputArchive &ar) const;
        void Load(JsonInputArchive &ar);

    private:
        GameObject *AllocateGameObject();
        GameObject                    *root;
        IRenderScene                  *renderScene;
        PmrUnSyncPoolRes               memoryResource;
        PmrVector<GameObject *>        gameObjects;
        PmrHashMap<Uuid, GameObject *> objectLut;
    };
    using WorldPtr = std::shared_ptr<World>;

} // namespace sky
