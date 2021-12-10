//
// Created by Zach Lee on 2021/12/9.
//

#pragma once

#include <framework/serialization/Any.h>
#include <framework/serialization/Type.h>
#include <type_traits>
#include <unordered_map>

namespace sky {

    using PropertyMap = std::unordered_map<uint32_t, Any>;
    using SetterFn = bool(*)(Any&, const Any&);
    using GetterFn = Any(*)(Any&);

    struct TypeMemberNode {
        TypeInfoRT *info = nullptr;
        const bool isConst;
        const bool isStatic;
        SetterFn setterFn = nullptr;
        GetterFn getterFn = nullptr;
        PropertyMap properties;
    };

    using MemberMap = std::unordered_map<std::string_view, TypeMemberNode>;
    struct TypeNode {
        TypeInfoRT* base = nullptr;
        TypeInfoRT* info = nullptr;
        MemberMap members;
        PropertyMap properties;
    };

    template <typename T, auto D>
    bool Setter(Any& any, const Any& value)
    {
        if constexpr(std::is_member_object_pointer_v<decltype(D)>) {
            using ValType = std::remove_reference_t<decltype(std::declval<T>().*D)>;

            if constexpr (!std::is_const_v<ValType>) {
                if (auto ptr = any.GetAs<T>(); ptr != nullptr) {
                    std::invoke(D, *ptr) = *value.GetAsConst<ValType>();
                    return true;
                }
            }
        }
        return false;
    }

    template <typename T, auto D>
    Any Getter(Any& any)
    {
        if constexpr(std::is_member_object_pointer_v<decltype(D)>) {
            if (auto ptr = any.GetAs<T>(); ptr != nullptr) {
                return Any(std::invoke(D, *ptr));
            }
        }
        return Any();
    }

    template<typename ...>
    class TypeFactory;

    template<typename T>
    class TypeFactory<T> {
    public:
        TypeFactory(TypeNode &node) : type(node)
        {
        }

        ~TypeFactory() = default;

        template <typename U>
        auto Base()
        {
            type.base = TypeInfoObj<U>::Get()->RtInfo();
        }

        template<auto M>
        auto Member(const std::string_view &key)
        {
            if constexpr(std::is_member_object_pointer_v<decltype(M)>) {
                return Member<M, M>(key);
            } else {
                using Type = std::remove_pointer_t<decltype(M)>;
                auto it = type.members.emplace(key, TypeMemberNode{
                    TypeInfoObj<Type>::Get()->RtInfo(),
                    std::is_const_v<Type>,
                    std::is_member_object_pointer_v<Type>,
                });
                return TypeFactory<T, std::integral_constant<decltype(M), M>>(type, it.first->second.properties);
            }
        }

        template <auto S, auto G>
        auto Member(const std::string_view &key)
        {
            using Type = std::remove_reference_t<std::invoke_result_t<decltype(G), T&>>;
            auto it = type.members.emplace(key, TypeMemberNode{
                TypeInfoObj<Type>::Get()->RtInfo(),
                std::is_const_v<Type>,
                !std::is_member_object_pointer_v<Type>,
                &Setter<T, S>,
                &Getter<T, G>
            });
            return TypeFactory<T, std::integral_constant<decltype(S), S>, std::integral_constant<decltype(G), G>>(type,
                it.first->second.properties);
        }

        auto operator()()
        {
            return TypeFactory<T, T>(type, type.properties);
        }

    protected:
        TypeNode &type;
    };

    template<typename T, typename ...S>
    class TypeFactory<T, S...> : public TypeFactory<T> {
    public:
        TypeFactory(TypeNode &node, PropertyMap &pMap) : TypeFactory<T>(node), property(pMap) {}

        TypeFactory &Property(uint32_t key, const Any &any)
        {
            property.emplace(key, any);
        }

    protected:
        PropertyMap &property;
    };
}