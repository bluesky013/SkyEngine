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

    class Actor;

    class JsonOutputArchive;

    class JsonInputArchive;

    class World {
    public:
        World();

        ~World();

        World(const World &) = delete;

        World &operator=(const World &) = delete;

        static void Reflect();

        void SetRenderScene(IRenderScene *scn) { renderScene = scn; }

        IRenderScene *GetRenderScene() const { return renderScene; }

        Actor *CreateActor(const std::string &name);

        Actor *CreateActor(const std::string &name, const Uuid &uuid);

        void DestroyActor(Actor *);

        void Tick(float);

        const PmrVector<Actor *> &GetActors() const;

        Actor *GetActorByUuid(const Uuid &id) const;

        Actor *GetRoot();

        void ForEachBFS(Actor *go, std::function<void(Actor *)> &&fn) const;

        void Save(JsonOutputArchive &ar) const;

        void Load(JsonInputArchive &ar);

    private:
        Actor *AllocateGameObject();

        Actor *root;
        IRenderScene *renderScene;
        PmrUnSyncPoolRes memoryResource;
        PmrVector<Actor *> actors;
        PmrHashMap<Uuid, Actor *> objectLut;
    };

    using WorldPtr = std::shared_ptr<World>;

} // namespace sky
