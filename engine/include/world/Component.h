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

    template <typename T>
    class ComponentBuilder {
    public:
        static_assert(std::is_base_of_v<Component, T>);
        ComponentBuilder() = default;
        ~ComponentBuilder() = default;

        static constexpr std::string_view name = TypeInfo<T>::Name();
        static constexpr uint32_t id = TypeInfo<T>::Hash();

        static T* CreateComponent()
        {
            return new T();
        }

        static std::string_view GetName()
        {
            return name;
        }

        static uint32_t GetId()
        {
            return id;
        }
    };

}