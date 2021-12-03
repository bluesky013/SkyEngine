//
// Created by Zach Lee on 2021/11/13.
//


#pragma once

#include <type_traits>
#include <string_view>
#include <core/util/Rtti.h>

namespace sky {

    class GameObject;

    class Component {
    public:
        Component() = default;
        virtual ~Component() = default;

    protected:
        friend class GameObject;
        GameObject* object = nullptr;
    };

    class ComponentBuilderBase {
    public:
        ComponentBuilderBase() = default;
        virtual ~ComponentBuilderBase() = default;

        virtual Component* CreateComponent() = 0;

        virtual std::string_view GetName() const = 0;

        virtual uint32_t GetId() const = 0;
    };

    template <typename T>
    class ComponentBuilder : public ComponentBuilderBase {
    public:
        static_assert(std::is_base_of_v<Component, T>);
        ComponentBuilder() = default;
        ~ComponentBuilder() = default;

        static constexpr std::string_view name = TypeInfo<T>::Name();
        static constexpr uint32_t id = TypeInfo<T>::Hash();

        Component* CreateComponent() override
        {
            return new T();
        }

        std::string_view GetName() const override
        {
            return name;
        }

        uint32_t GetId() const override
        {
            return id;
        }
    };

}