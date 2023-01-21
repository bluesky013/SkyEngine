//
// Created by Zach Lee on 2021/11/13.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/type/Rtti.h>
#include <core/type/Type.h>
#include <core/std/Container.h>
#include <core/util/Uuid.h>
#include <framework/serialization/SerializationContext.h>
#include <set>
#include <string_view>
#include <type_traits>

namespace sky {

    class GameObject;
    class JsonOutputArchive;
    class JsonInputArchive;

    class Component {
    public:
        Component()          = default;
        virtual ~Component() = default;

        TYPE_RTTI_BASE

        GameObject *object = nullptr;

        virtual void OnActive()
        {
        }

        virtual void OnTick(float time)
        {
        }

        virtual void OnDestroy()
        {
        }

        virtual Uuid GetType() const = 0;
        virtual std::string_view GetTypeStr() const = 0;

        virtual void Save(JsonOutputArchive &ar) const {}
        virtual void Load(JsonInputArchive &ar) {}

    protected:
        friend class GameObject;
    };

    struct IComponentListener {
        virtual void OnAddComponent(GameObject *go, Component *)
        {
        }
        virtual void OnRemoveComponent(GameObject *go, Component *)
        {
        }
    };

    template <typename T>
    class ComponentFactory : public Singleton<ComponentFactory<T>> {
    public:
        static_assert(std::is_base_of_v<Component, T>);
        ComponentFactory()  = default;
        ~ComponentFactory() = default;

        static constexpr std::string_view name = TypeInfo<T>::Name();
        static constexpr uint32_t         id   = TypeInfo<T>::Hash();

        static T *CreateComponent(void *ptr)
        {
            return new (ptr) T();
        }

        static std::string_view GetName()
        {
            return name;
        }

        static uint32_t GetId()
        {
            return id;
        }

        template <auto F>
        void ForEach(GameObject *go, Component *component)
        {
            for (auto &listener : listeners) {
                std::invoke(F, listener, go, static_cast<T *>(component));
            }
        }

        void RegisterListener(IComponentListener *listener)
        {
            if (listener != nullptr) {
                listeners.emplace(listener);
            }
        }

        void UnRegisterListener(IComponentListener *listener)
        {
            listeners.erase(listener);
        }

    private:
        std::set<IComponentListener *> listeners;
    };

} // namespace sky
