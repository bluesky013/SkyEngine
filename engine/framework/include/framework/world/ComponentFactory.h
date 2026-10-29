//
// Created by blues on 2024/4/19.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/name/Name.h>
#include <core/util/Uuid.h>
#include <framework/world/Component.h>
#include <memory>
#include <type_traits>
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
            const TypeInfoRT *(*queryDataType)()       = nullptr;
            std::vector<Name>           requiredSubSystems;
            std::shared_ptr<const void> moduleLease;
        };

        template <typename T>
        void RegisterComponent(const std::string &group, std::vector<Name> requiredSubSystems = {})
        {
            RegisterComponent<T>(*SerializationContext::Get(), group, std::move(requiredSubSystems));
        }

        // Core metadata bootstrap uses its constructing context without reentering the singleton.
        template <typename T>
        void RegisterComponent(SerializationContext &context, const std::string &group, std::vector<Name> requiredSubSystems = {})
        {
            static_assert(std::is_base_of_v<ComponentBase, T>);

            const auto *info = TypeInfoObj<T>::Get()->RtInfo();
            SKY_ASSERT(Find(info->registeredId) == nullptr)
            componentTypes[group].push_back({
                info->registeredId,
                std::string(info->name),
                []() -> std::unique_ptr<ComponentBase> {
                    if constexpr (std::is_default_constructible_v<T>) {
                        return std::make_unique<T>();
                    }
                    return {};
                },
                []() -> const TypeInfoRT * {
                    if constexpr (requires { typename T::PersistentDataType; }) {
                        return TypeInfoObj<typename T::PersistentDataType>::Get()->RtInfo();
                    }
                    return nullptr;
                },
                std::move(requiredSubSystems),
                context.RetainModuleCode(),
            });
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

        const TypeInfoRT *GetPersistentDataType(const Uuid &id) const
        {
            const auto *info = Find(id);
            return info && info->queryDataType ? info->queryDataType() : nullptr;
        }

        const std::unordered_map<std::string, std::vector<ComponentInfo>> &GetTypes() const
        {
            return componentTypes;
        }

    private:
        std::unordered_map<std::string, std::vector<ComponentInfo>> componentTypes;
    };

} // namespace sky
