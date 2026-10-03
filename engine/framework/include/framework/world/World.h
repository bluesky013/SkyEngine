//
// Created by Zach Lee on 2021/11/12.
//

#pragma once

#include <core/util/Uuid.h>
#include <core/std/Container.h>
#include <core/event/Event.h>
#include <core/template/ReferenceObject.h>
#include <core/name/Name.h>
#include <framework/world/Actor.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/BinaryArchive.h>

#include <string>
#include <vector>
#include <memory>
#include <functional>


namespace sky {

    class World;
    using WorldPtr = CounterPtr<World>;

    class IWorldEvent {
    public:
        IWorldEvent() = default;
        ~IWorldEvent() = default;

        using KeyType   = World*;
        using MutexType = void;

        virtual void OnActorAttached(Actor *actor) = 0;
        virtual void OnActorDetached(Actor *actor) = 0;
    };
    using WorldEvent = Event<IWorldEvent>;

    class IWorldSubSystem {
    public:
        IWorldSubSystem() = default;
        virtual ~IWorldSubSystem() = default;

        virtual void OnAttachToWorld(World &world) {}
        virtual void OnDetachFromWorld(World &world) {}

        virtual void StartSimulation() {}
        virtual void StopSimulation() {}

        virtual void Tick(float time) {}
    };

    class World : public RefObject {
    public:
        ~World() override;

        static World *CreateWorld();

        World(const World &) = delete;
        World &operator=(const World &) = delete;

        static void Reflect(SerializationContext *context);

        void Init();
        void Tick(float time);

        void SetPersistID(const Uuid& inID) { persistID = inID; }
        const Uuid& GetPersistID() const { return persistID; }

        void SaveJson(JsonOutputArchive &archive);
        void LoadJson(JsonInputArchive &archive);

        Actor *CreateActor(bool withTrans = true);
        Actor *CreateActor(const char *name, bool withTrans = true);
        Actor *CreateActor(const std::string &name, bool withTrans = true);
        Actor *CreateActor(const Uuid &id, bool withTrans = true);
        Actor *GetActorByUuid(const Uuid &id);
        const std::vector<std::unique_ptr<Actor>> &GetActors() const { return actors; }

        // Takes ownership; the actor must not already belong to a world.
        Actor *AttachToWorld(std::unique_ptr<Actor> actor);
        // Releases ownership of the actor (returns it) so the caller can retain or move it.
        std::unique_ptr<Actor> DetachFromWorld(Actor *actor);
        void Reset();

        void AddSubSystem(const Name &name, IWorldSubSystem*);
        IWorldSubSystem* GetSubSystem(const Name &name) const;

        void RegisterConfiguration(const Name &name, const Any& any);
        const Any& GetConfigByName(const Name &name) const;

    private:
        World() = default;

        std::vector<std::unique_ptr<Actor>> actors;
        std::unordered_map<Uuid, size_t> actorIndex;
        std::unordered_map<Name, std::unique_ptr<IWorldSubSystem>> subSystems;

        Uuid persistID;

        std::unordered_map<Name, Any> worldConfigs;
    };
} // namespace sky
