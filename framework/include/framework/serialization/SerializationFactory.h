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

    struct TypeMemberNode {
        TypeInfoRT *info = nullptr;
        const bool isConst;
        const bool isStatic;
        PropertyMap properties;
    };

    using MemberMap = std::unordered_map<std::string_view, TypeMemberNode>;
    struct TypeNode {
        TypeInfoRT* base = nullptr;
        TypeInfoRT* info = nullptr;
        MemberMap members;
        PropertyMap properties;
    };

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
                using Type = std::remove_reference_t<std::invoke_result_t<decltype(M), T&>>;
                auto it = type.members.emplace(key, TypeMemberNode{
                    TypeInfoObj<Type>::Get()->RtInfo(),
                    std::is_const_v<Type>,
                    !std::is_member_object_pointer_v<Type>,
                });
                return TypeFactory<T, std::integral_constant<decltype(M), M>>(type, it.first->second.properties);
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