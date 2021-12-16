//
// Created by Zach Lee on 2021/11/13.
//


#pragma once

#include <type_traits>
#include <string_view>
#include <core/util/Rtti.h>
#include <framework/serialization/Type.h>
#include <framework/serialization/SerializationContext.h>

namespace sky {

    class GameObject;

    class Component {
    public:
        Component() = default;
        virtual ~Component() = default;

        TYPE_RTTI_BASE

        GameObject* object = nullptr;

    protected:
        friend class GameObject;
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