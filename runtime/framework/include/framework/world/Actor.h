//
// Created by blues on 2024/5/14.
//

#pragma once

#include <framework/serialization/SerializationContext.h>
#include <framework/world/Component.h>
#include <core/platform/Platform.h>
#include <list>
#include <unordered_map>
#include <memory>

namespace sky {

    class World;

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
            if (!AddComponent(id, component)) {
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
        bool AddComponent(const Uuid &typeId, ComponentBase* component);
        void RemoveComponent(const Uuid &typeId);

        void SaveJson(JsonOutputArchive &archive);
        void LoadJson(JsonInputArchive &archive);

        void Tick(float time);

        const Uuid &GetUuid() const { return uuid; }
        const std::string &GetName() const { return name; }
        void SetName(const std::string &name_) { name = name_; }
        World *GetWorld() const { return world; }

        const std::unordered_map<Uuid, ComponentPtr> &GetComponents() const { return storage; }

    private:
        friend class World;


        std::unordered_map<Uuid, ComponentPtr> storage;

        Uuid uuid;
        std::string name;

        World *world = nullptr;
    };

} // namespace sky
