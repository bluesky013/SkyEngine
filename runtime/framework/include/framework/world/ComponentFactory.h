//
// Created by blues on 2024/4/19.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/std/Container.h>

namespace sky {
    class Component;

    class ComponentFactory : public Singleton<ComponentFactory> {
    public:
        ComponentFactory()  = default;
        ~ComponentFactory() override = default;

        template <typename T>
        static Component *CreateComponent(PmrResource *resource)
        {
            auto *ptr = resource->allocate(sizeof(T));
            return new (ptr) T();
        }
        using CompFn = Component*(*)(PmrResource *resource);
        struct CompInfo {
            CompFn fn;
            std::string_view name;
        };

        template <typename T>
        void RegisterComponent()
        {
            ctorMap.emplace(T::TYPE, &ComponentFactory::CreateComponent<T>);
        }

        Component *CreateComponent(PmrResource *resource, const uint32_t &id)
        {
            auto iter = ctorMap.find(id);
            if (iter != ctorMap.end()) {
                return iter->second(resource);
            }
            return nullptr;
        }

        const std::unordered_map<uint32_t, CompFn> &GetComponentTypes() const
        {
            return ctorMap;
        }

    private:
        std::unordered_map<uint32_t, CompFn> ctorMap;
    };

} // namespace sky