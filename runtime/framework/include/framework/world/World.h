//
// Created by Zach Lee on 2021/11/12.
//

#pragma once

#include <core/util/Uuid.h>
#include <core/math/Rect.h>
#include <core/std/Container.h>
#include <core/event/Event.h>
#include <framework/world/Entity.h>
#include <framework/world/Actor.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/BinaryArchive.h>

#include <string>
#include <vector>
#include <functional>


namespace sky {

    class World;

    class IWorldEvent {
    public:
        IWorldEvent() = default;
        ~IWorldEvent() = default;

        using KeyType   = void;
        using MutexType = void;

        virtual void OnCreateWorld(World& world) = 0;
        virtual void OnDestroyWorld(World& world) = 0;
    };
    using WorldEvent = Event<IWorldEvent>;

    class WorldSubSystem {
    public:
        WorldSubSystem() = default;
        virtual ~WorldSubSystem() = default;

        virtual void OnTick(float time) = 0;
    };

    class IWorldSubSystem {
    public:
        IWorldSubSystem() = default;
        virtual ~IWorldSubSystem() = default;
    };

    class World {
    public:
        ~World();

        static World *CreateWorld();

        World(const World &) = delete;
        World &operator=(const World &) = delete;

        using ActorPtr = std::unique_ptr<Actor>;

        static void Reflect(SerializationContext *context);

        bool Init();
        void Tick(float time);

        void SaveJson(JsonOutputArchive &archive);
        void LoadJson(JsonInputArchive &archive);

        void SaveBinary(BinaryOutputArchive &archive);
        void LoadBinary(BinaryInputArchive &archive);

        Actor* CreateActor();
        Actor* CreateActor(const std::string &name);
        Actor* CreateActor(const Uuid &id);
        Actor* GetActorByUuid(const Uuid &id);
        const std::vector<ActorPtr> &GetActors() const { return actors; }

        void DestroyActor(Actor *);
        void Reset();

        void AddSubSystem(const std::string &name, IWorldSubSystem*);
        IWorldSubSystem* GetSubSystem(const std::string &name) const;

    private:
        World() = default;

        std::vector<ActorPtr> actors;
        std::unordered_map<std::string, IWorldSubSystem*> subSystems;

        uint32_t version = 0;
    };

    using WorldPtr = std::shared_ptr<World>;

} // namespace sky
