//
// Created by Zach Lee on 2021/12/9.
//

#pragma once

#include <framework/serialization/Any.h>
#include <framework/serialization/Type.h>
#include <type_traits>
#include <unordered_map>
#include <list>

namespace sky {

    using PropertyMap = std::unordered_map<uint32_t, Any>;
    using SetterFn = bool(*)(void* ptr, const Any&);
    using GetterFn = Any(*)(void* ptr, bool asRef);
    using GetterConstFn = Any(*)(const void* ptr);
    using ConstructFn = Any(*)(Any*);

    struct TypeMemberNode {
        TypeInfoRT *info = nullptr;
        const bool isConst;
        const bool isStatic;
        SetterFn setterFn = nullptr;
        GetterFn getterFn = nullptr;
        GetterConstFn getterConstFn = nullptr;
        PropertyMap properties;
    };

    struct ConstructNode {
        const uint32_t argsNum;
        ConstructFn constructFn = nullptr;
    };

    using MemberMap = std::unordered_map<std::string_view, TypeMemberNode>;
    using ConstructList = std::list<ConstructNode>;
    struct TypeNode {
        TypeInfoRT* base = nullptr;
        TypeInfoRT* info = nullptr;
        MemberMap members;
        PropertyMap properties;
        ConstructList constructList;
    };

    template <typename T, auto D>
    bool Setter(void* p, const Any& value)
    {
        if constexpr(std::is_member_object_pointer_v<decltype(D)>) {
            using ValType = std::remove_reference_t<decltype(std::declval<T>().*D)>;

            if constexpr (!std::is_const_v<ValType>) {
                if (auto ptr = static_cast<T*>(p); ptr != nullptr) {
                    std::invoke(D, *ptr) = *value.GetAsConst<ValType>();
                    return true;
                }
            }
        }
        return false;
    }

    template <typename T, auto D>
    Any Getter(void* p, bool asRef)
    {
        if constexpr(std::is_member_object_pointer_v<decltype(D)>) {
            if (auto ptr = static_cast<T*>(p); ptr != nullptr) {
                if (asRef) {
                    return std::ref(std::invoke(D, *ptr));
                } else {
                    return Any(std::invoke(D, *ptr));
                }
            }
        }
        return Any();
    }

    template <typename T, auto D>
    Any GetterConst(const void* p)
    {
        if constexpr(std::is_member_object_pointer_v<decltype(D)>) {
            if (auto ptr = static_cast<const T*>(p); ptr != nullptr) {
                return Any(std::invoke(D, *ptr));
            }
        }
        return Any();
    }

    template <typename ...>
    struct FuncTraits;

    template <typename Ret, typename Cls, typename ...Args>
    struct FuncTraits<Ret(Cls::*)(Args...)> {
        using RET_TYPE = Ret;
        using ARGS_TYPE = std::tuple<Cls*, Args...>;
        static constexpr bool CONST = false;
    };

    template <typename Ret, typename Cls, typename ...Args>
    struct FuncTraits<Ret(Cls::*)(Args...)const> {
        using RET_TYPE = Ret;
        using ARGS_TYPE = std::tuple<Cls*, Args...>;
        static constexpr bool CONST = true;
    };

    template <typename Ret, typename ...Args>
    struct FuncTraits<Ret(Args...)> {
        using RET_TYPE = Ret;
        using ARGS_TYPE = std::tuple<Args...>;
    };

    template <typename T, typename ...Args, size_t ...I>
    Any Construct(Any* args, std::index_sequence<I...>) {
        if (((args[I].GetAs<Args>() != nullptr) && ...)) {
            return Any(std::in_place_type<T>, *args[I].GetAs<Args>()...);
        }
        return {};
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

        template <typename ...Args>
        TypeFactory& Constructor()
        {
            using FuncType = FuncTraits<Any(Args...)>;
            using ArgsType = typename FuncType::ARGS_TYPE;

            type.constructList.emplace_back(ConstructNode {
                std::tuple_size_v<ArgsType>,
                [](Any* args) -> Any {
                    return Construct<T, Args...>(args, std::make_index_sequence<std::tuple_size_v<ArgsType>>{});
                }
            });
            return *this;
        }

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
                &Getter<T, G>,
                &GetterConst<T, G>,
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
            return *this;
        }

    protected:
        PropertyMap &property;
    };
}