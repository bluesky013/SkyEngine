//
// Created by Zach Lee on 2021/11/12.
//

#pragma once

#include <core/util/Uuid.h>
#include <core/std/Container.h>
#include <core/event/Event.h>
#include <core/template/ReferenceObject.h>
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

        virtual void OnCreateWorld(const WorldPtr& world) = 0;
        virtual void OnDestroyWorld(const WorldPtr& world) = 0;
    };
    using WorldEvent = Event<IWorldEvent>;

    class IWorldSubSystem {
    public:
        IWorldSubSystem() = default;
        virtual ~IWorldSubSystem() = default;

        virtual void OnAttachToWorld(World &world) {}

        virtual void Tick(float time) {}
    };

    class World : public RefObject {
    public:
        ~World() override;

        static World *CreateWorld();

        World(const World &) = delete;
        World &operator=(const World &) = delete;

        static void Reflect(SerializationContext *context);

        bool Init();
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

        void AddSubSystem(const std::string &name, IWorldSubSystem*);
        IWorldSubSystem* GetSubSystem(const std::string &name) const;

        void RegisterConfiguration(const std::string& name, const Any& any);
        const Any& GetConfigByName(const std::string &name) const;

    private:
        World() = default;

        std::vector<ActorPtr> actors;
        std::unordered_map<std::string, IWorldSubSystem*> subSystems;

        std::unordered_map<std::string, Any> worldConfigs;

        uint32_t version = 0;
    };
} // namespace sky
