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
            Uuid typeId;
            std::string_view name;
        };

        template <typename T>
        void RegisterComponent(const std::string &group)
        {
            static_assert(std::is_base_of_v<ComponentBase, T>);

            const auto *info = TypeInfoObj<T>::Get()->RtInfo();

            RegisterComponent(info->registeredId, info->name, group);
        }

        const std::unordered_map<std::string, std::vector<ComponentInfo>> &GetTypes() const { return componentTypes; }

    private:
        void RegisterComponent(const Uuid &uuid, const std::string_view &name, const std::string &group)
        {
            componentTypes[group].emplace_back(uuid, name);
        }

        std::unordered_map<std::string, std::vector<ComponentInfo>> componentTypes;
    };

} // namespace sky