//
// Created by Zach Lee on 2021/11/12.
//

#pragma once

#include <core/util/Uuid.h>
#include <core/std/Container.h>
#include <core/event/Event.h>
#include <core/template/ReferenceObject.h>
#include <core/name/Name.h>
#include <framework/world/Entity.h>
#include <framework/world/Actor.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/BinaryArchive.h>

#include <string>
#include <vector>
#include <functional>


namespace sky {

    class World;
    using WorldPtr = CounterPtr<World>;

    class IWorldEvent {
    public:
        IWorldEvent() = default;
        ~IWorldEvent() = default;

        using KeyType   = void;
        using MutexType = void;

        virtual void OnCreateWorld(World& world) = 0;
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

        void SaveJson(JsonOutputArchive &archive);
        void LoadJson(JsonInputArchive &archive);

        ActorPtr CreateActor(bool withTrans = true);
        ActorPtr CreateActor(const char *name, bool withTrans = true);
        ActorPtr CreateActor(const std::string &name, bool withTrans = true);
        ActorPtr CreateActor(const Uuid &id, bool withTrans = true);
        ActorPtr GetActorByUuid(const Uuid &id);
        const std::vector<ActorPtr> &GetActors() const { return actors; }

        void AttachToWorld(const ActorPtr &);
        void DetachFromWorld(const ActorPtr &);
        void Reset();

        void AddSubSystem(const Name &name, IWorldSubSystem*);
        IWorldSubSystem* GetSubSystem(const Name &name) const;

        void RegisterConfiguration(const Name &name, const Any& any);
        const Any& GetConfigByName(const Name &name) const;

    private:
        World() = default;

        std::vector<ActorPtr> actors;
        std::unordered_map<Name, std::unique_ptr<IWorldSubSystem>> subSystems;

        std::unordered_map<Name, Any> worldConfigs;
        uint32_t version = 0;
    };
} // namespace sky
