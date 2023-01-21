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

    class ComponentFactory : public Singleton<ComponentFactory> {
    public:
        ComponentFactory()  = default;
        ~ComponentFactory() = default;

        template <typename T>
        static Component *CreateComponent(PmrResource *resource)
        {
            auto ptr = resource->allocate(sizeof(T));
            return new (ptr) T();
        }
        using CompFn = Component*(*)(PmrResource *resource);

        template <auto F>
        void ForEach(GameObject *go, Component *component)
        {
            for (auto &listener : listeners) {
                std::invoke(F, listener, go, component);
            }
        }

        template <typename T>
        void RegisterComponent()
        {
            ctorMap.emplace(T::TYPE, &ComponentFactory::CreateComponent<T>);
        }

        Component *CreateComponent(PmrResource *resource, const Uuid &id)
        {
            auto iter = ctorMap.find(id);
            if (iter != ctorMap.end()) {
                return iter->second(resource);
            }
            return nullptr;
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
        std::unordered_map<Uuid, CompFn> ctorMap;
    };

} // namespace sky
