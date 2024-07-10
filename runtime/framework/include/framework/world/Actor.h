//
// Created by blues on 2024/5/14.
//

#pragma once

#include <core/platform/Platform.h>

#include <framework/serialization/SerializationContext.h>
#include <framework/world/Component.h>

#include <list>
#include <unordered_map>
#include <memory>

namespace sky {

    class Actor;
    class World;

    using ActorPtr = std::shared_ptr<Actor>;
    using ActorWeakPtr = std::weak_ptr<Actor>;

    class Actor {
    public:
        Actor() = default;
        explicit Actor(Uuid id) : uuid(id), name("Actor") {}
        ~Actor() = default;

        using ComponentPtr = std::unique_ptr<ComponentBase>;

        template <typename T, typename ...Args>
        T* AddComponent(Args &&...args)
        {
            static_assert(std::is_base_of_v<ComponentBase, T>);
            const auto &id = TypeInfoObj<T>::Get()->RtInfo()->registeredId;
            SKY_ASSERT(static_cast<bool>(id));

            auto *component = new T(std::forward<Args>(args)...);
            if (!EmplaceComponent(id, component)) {
                delete component;
                component = nullptr;
            }
            component->actor = this;
            component->OnActive();
            return component;
        }

        template <typename T>
        T* GetComponent()
        {
            static_assert(std::is_base_of_v<ComponentBase, T>);
            const auto &id = TypeInfoObj<T>::Get()->RtInfo()->registeredId;
            SKY_ASSERT(static_cast<bool>(id));

            return static_cast<T*>(GetComponent(id));
        }

        template <typename T>
        void RemoveComponent()
        {
            static_assert(std::is_base_of_v<ComponentBase, T>);
            const auto &id = TypeInfoObj<T>::Get()->RtInfo()->registeredId;
            SKY_ASSERT(static_cast<bool>(id));

            RemoveComponent(id);
        }

        ComponentBase *GetComponent(const Uuid &typeId);
        ComponentBase *AddComponent(const Uuid &typeId);
        void RemoveComponent(const Uuid &typeId);

        void SaveJson(JsonOutputArchive &archive);
        void LoadJson(JsonInputArchive &archive);

        void SetParent(const ActorPtr &actor);

        void Tick(float time);

        const Uuid &GetUuid() const { return uuid; }
        const std::string &GetName() const { return name; }
        void SetName(const std::string &name_) { name = name_; }
        World *GetWorld() const { return world; }

        const std::unordered_map<Uuid, ComponentPtr> &GetComponents() const { return storage; }

    private:
        friend class World;
        bool EmplaceComponent(const Uuid &typeId, ComponentBase* component);

        std::unordered_map<Uuid, ComponentPtr> storage;

        Uuid uuid;
        std::string name;

        World *world = nullptr;
    };

} // namespace sky
