//
// Created by blues on 2024/4/19.
//

#pragma once

#include <algorithm>
#include <core/environment/Singleton.h>
#include <core/name/Name.h>
#include <core/util/Uuid.h>
#include <framework/world/Component.h>
#include <vector>

namespace sky {
    class ComponentFactory : public Singleton<ComponentFactory> {
    public:
        ComponentFactory()           = default;
        ~ComponentFactory() override = default;

        struct ComponentInfo {
            Uuid        typeId;
            std::string name;
            std::unique_ptr<ComponentBase> (*create)() = nullptr;
            std::vector<Name>           requiredSubSystems;
            std::shared_ptr<const void> moduleLease;
        };

        template <typename T>
        void RegisterComponent(const std::string &group, std::vector<Name> requiredSubSystems = {})
        {
            static_assert(std::is_base_of_v<ComponentBase, T>);

            const auto *info = TypeInfoObj<T>::Get()->RtInfo();

            SKY_ASSERT(Find(info->registeredId) == nullptr)
            componentTypes[group].push_back({info->registeredId, std::string(info->name),
                                             []() -> std::unique_ptr<ComponentBase> { return std::make_unique<T>(); }, std::move(requiredSubSystems),
                                             SerializationContext::Get()->RetainModuleCode()});
        }

        const ComponentInfo *Find(const Uuid &id) const
        {
            for (const auto &[group, types] : componentTypes) {
                for (const auto &type : types) {
                    if (type.typeId == id) {
                        return &type;
                    }
                }
            }
            return nullptr;
        }

        std::unique_ptr<ComponentBase> Create(const Uuid &id) const
        {
            const auto *info = Find(id);
            return info && info->create ? info->create() : nullptr;
        }

        const std::unordered_map<std::string, std::vector<ComponentInfo>> &GetTypes() const
        {
            return componentTypes;
        }

    private:
        std::unordered_map<std::string, std::vector<ComponentInfo>> componentTypes;
    };

} // namespace sky
