//
// Created by blues on 2024/4/19.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Uuid.h>
#include <framework/world/Component.h>
#include <vector>

namespace sky {
    class ComponentFactory : public Singleton<ComponentFactory> {
    public:
        ComponentFactory()  = default;
        ~ComponentFactory() override = default;

        struct ComponentInfo {
            std::string group;
        };

        template <typename T>
        void RegisterComponent(const std::string &group)
        {
            static_assert(std::is_base_of_v<ComponentBase, T>);
            RegisterComponent(TypeInfo<T>::RegisteredId(), ComponentInfo{group});
        }

    private:
        void RegisterComponent(const Uuid &uuid, const ComponentInfo &info)
        {
            componentTypes.emplace(uuid, info);
        }
        std::unordered_map<Uuid, ComponentInfo> componentTypes;
    };

} // namespace sky