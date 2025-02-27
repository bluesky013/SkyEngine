//
// Created by Zach Lee on 2021/12/9.
//

#pragma once

#include <framework/serialization/Any.h>
#include <core/type/Type.h>
#include <list>
#include <type_traits>
#include <map>

namespace sky {
    class JsonInputArchive;
    class JsonOutputArchive;

    class BinaryInputArchive;
    class BinaryOutputArchive;

    namespace serialize {
        using PropertyMap = std::unordered_map<uint32_t, Any>;

        using SetterFn = bool (*)(void *ptr, const void *);
        using GetterFn = Any (*)(void *ptr);
        using GetterConstFn = Any (*)(const void *ptr);
        using ValueChangedFn = void (*)(void *ptr);
        using ConstructibleFn = bool (*)(Any *);
        using ConstructFn = Any (*)(Any *);
        using JsonInFn = void (*)(void *p, JsonInputArchive &archive);
        using JsonOutFn = void (*)(const void *p, JsonOutputArchive &archive);
        using BinaryInFn = void (*)(void *p, BinaryInputArchive &archive);
        using BinaryOutFn = void (*)(const void *p, BinaryOutputArchive &archive);
        using MemberFun = Any (*)(void *ptr, Any *);
        using CheckMemberFunc = bool (*)(Any *);

        struct TypeMemberNode {
            const TypeInfoRT *info = nullptr;
            const bool isConst;
            const bool isStatic;
            SetterFn setterFn = nullptr;
            GetterFn getterFn = nullptr;
            GetterConstFn getterConstFn = nullptr;
            ValueChangedFn valueChangedFn = nullptr;
            PropertyMap properties;
        };

        struct ConstructNode {
            const uint32_t argsNum;
            ConstructibleFn checkFn = nullptr;
            ConstructFn constructFn = nullptr;
        };

        struct MemberFunctionNode {
            const uint32_t argsNum;
            CheckMemberFunc checkFn = nullptr;
            MemberFun memberFun = nullptr;
        };

        struct SerializationNode {
            JsonInFn jsonLoad = nullptr;
            JsonOutFn jsonSave = nullptr;
            BinaryInFn binaryLoad = nullptr;
            BinaryOutFn binarySave = nullptr;
        };

        using MemberMap = std::map<std::string_view, TypeMemberNode>;
        using FunctionMap = std::map<std::string_view, MemberFunctionNode>;
        using EnumMap = std::map<uint64_t, std::string_view>;
        using ConstructList = std::list<ConstructNode>;

    } // namespace serialize
    struct TypeNode {
        const TypeInfoRT  *base = nullptr;
        const TypeInfoRT  *info = nullptr;
        serialize::MemberMap         members;
        serialize::EnumMap           enums;
        serialize::PropertyMap       properties;
        serialize::ConstructList     constructList;
        serialize::SerializationNode serialization;
        serialize::FunctionMap       functions;
    };

    template <auto Func, typename Archive>
    auto SerializationInArchive()
    {
        using Type = decltype(Func);
        using RetType = void (*)(void *p, Archive& archive);
        if constexpr (std::is_member_function_pointer_v<Type>) {
            using Cls = typename FuncTraits<Type>::CLASS_TYPE;
            return RetType{[](void* p, Archive &archive) {
                Cls *ptr = static_cast<Cls*>(p);
                std::invoke(Func, ptr, archive);
            }};
        } else {
            using Args = typename FuncTraits<Type>::ARGS_TYPE;
            using Cls = std::remove_const_t<std::remove_reference_t<typename std::tuple_element_t<0, Args>>>;
            return RetType{[](void* p, Archive &archive) {
                Cls *ptr = static_cast<Cls*>(p);
                std::invoke(Func, *ptr, archive);
            }};
        }
    }

    template <auto Func, typename Archive>
    auto SerializationOutArchive()
    {
        using Type    = decltype(Func);
        using RetType = void (*)(const void *p, Archive &archive);
        if constexpr (std::is_member_function_pointer_v<Type>) {
            using Cls = typename FuncTraits<Type>::CLASS_TYPE;
            return RetType{[](const void *p, Archive &archive) {
                const Cls *ptr = static_cast<const Cls *>(p);
                std::invoke(Func, ptr, archive);
            }};
        } else {
            using Args = typename FuncTraits<Type>::ARGS_TYPE;
            using Cls = std::remove_const_t<std::remove_reference_t<typename std::tuple_element_t<0, Args>>>;
            return RetType{[](const void* p, Archive &archive) {
                const Cls *ptr = static_cast<const Cls*>(p);
                std::invoke(Func, *ptr, archive);
            }};
        }
    }

    template <typename T, typename V, auto D>
    bool Setter(void *p, const void *value) noexcept
    {
        if constexpr (std::is_member_object_pointer_v<decltype(D)>) {
            if constexpr (!std::is_const_v<V>) {
                if (auto ptr = static_cast<T *>(p); ptr != nullptr) {
                    std::invoke(D, *ptr) = *static_cast<const V *>(value);
                    return true;
                }
            }
        } else if constexpr (std::is_member_function_pointer_v<decltype(D)>) {
            std::invoke(D, static_cast<T *>(p), *static_cast<const V*>(value));
            return true;
        }
        return false;
    }

    template <typename T, auto D>
    Any Getter(void *p) noexcept
    {
        if constexpr (std::is_member_object_pointer_v<decltype(D)> || std::is_member_function_pointer_v<decltype(D)>) {
            return Any(std::invoke(D, static_cast<T*>(p)));
        }
        return Any{};
    }

    template <typename T, auto D>
    Any GetterConst(const void *p) noexcept
    {
        if constexpr (std::is_member_object_pointer_v<decltype(D)> || std::is_member_function_pointer_v<decltype(D)>) {
            if constexpr (std::is_invocable_v<decltype(D), const T*>) {
                return Any(std::invoke(D, static_cast<const T*>(p)));
            }
        }
        return Any{};
    }

    template <typename T, auto D>
    void ValueChanged_(void *p) noexcept
    {
        static_assert(std::is_member_function_pointer_v<decltype(D)>);
        std::invoke(D, static_cast<T*>(p));
    }

    template <typename T, typename... Args, size_t... I>
    bool ConstructCheck(Any *args, std::index_sequence<I...>)
    {
        return ((args[I].GetAs<Args>() != nullptr) && ...);
    }

    template <typename T, typename... Args, size_t... I>
    Any Construct(Any *args, std::index_sequence<I...>)
    {
        return Any(std::in_place_type<T>, *args[I].GetAs<Args>()...);
    }

    template <auto F, typename Tuple, typename Cls, size_t... I>
    Any InvokeMemberFunc(Cls* val, Any *args, std::index_sequence<I...>)
    {
        return Any(std::invoke(F, val, *args[I].GetAs<std::tuple_element_t<I, Tuple>>()...));
    }

    template <typename...>
    class TypeFactory;

    template <typename T>
    class TypeFactory<T> {
    public:
        explicit TypeFactory(TypeNode &node) : type(node)
        {
        }

        ~TypeFactory() = default;

        template <typename... Args>
        TypeFactory &Constructor()
        {
            using FuncType = FuncTraits<Any(*)(Args...)>;
            using ArgsType = typename FuncType::ARGS_TYPE;

            type.constructList.emplace_back(
                serialize::ConstructNode{std::tuple_size_v<ArgsType>,
                              [](Any *args) -> bool { return ConstructCheck<T, Args...>(args, std::make_index_sequence<std::tuple_size_v<ArgsType>>{}); },
                              [](Any *args) -> Any { return Construct<T, Args...>(args, std::make_index_sequence<std::tuple_size_v<ArgsType>>{}); }});
            return *this;
        }

        template <typename U>
        auto Base()
        {
            type.base = TypeInfoObj<U>::Get()->RtInfo();
        }

        template <auto M>
        auto Member(const std::string_view &key)
        {
            if constexpr (std::is_member_object_pointer_v<decltype(M)>) {
                return Member<M, M>(key);
            } else {
                using Type = std::remove_pointer_t<decltype(M)>;
                auto it    = type.members.emplace(key, serialize::TypeMemberNode{
                                                        TypeInfoObj<std::remove_const_t<Type>>::Get()->RtInfo(),
                                                        std::is_const_v<Type>,
                                                        std::is_member_object_pointer_v<Type>,
                                                    });
                return TypeFactory<T, std::integral_constant<decltype(M), M>>(type, it.first->second.properties);
            }
        }

        template <auto S, auto G>
        auto Member(const std::string_view &key)
        {
            using Type = std::remove_reference_t<std::invoke_result_t<decltype(G), T &>>;
            if constexpr (std::is_const_v<Type>) {
                auto it    = type.members.emplace(key, serialize::TypeMemberNode{
                                                        TypeInfoObj<std::remove_const_t<Type>>::Get()->RtInfo(),
                                                        std::is_const_v<Type>,
                                                        !std::is_member_object_pointer_v<Type>,
                                                        &Setter<T, Type, S>,
                                                        nullptr,
                                                        &GetterConst<T, G>,
                                                        nullptr
                                                    });
                if constexpr (ContainerTraits<Type>::IS_SEQUENCE) {
                    it.first->second.info->containerInfo->valueType =
                            TypeInfoObj<std::remove_cv_t<typename SequenceContainerMap<Type>::ValueType>>::Get()->RtInfo()->registeredId;
                }
                return TypeFactory<T, std::integral_constant<decltype(S), S>, std::integral_constant<decltype(G), G>>(type, it.first->second.properties);
            } else {
                auto it    = type.members.emplace(key, serialize::TypeMemberNode{
                                                        TypeInfoObj<std::remove_const_t<Type>>::Get()->RtInfo(),
                                                        std::is_const_v<Type>,
                                                        !std::is_member_object_pointer_v<Type>,
                                                        &Setter<T, Type, S>,
                                                        &Getter<T, G>,
                                                        &GetterConst<T, G>,
                                                        nullptr
                                                    });
                if constexpr (ContainerTraits<Type>::IS_SEQUENCE) {
                    it.first->second.info->containerInfo->valueType =
                            TypeInfoObj<std::remove_cv_t<typename SequenceContainerMap<Type>::ValueType>>::Get()->RtInfo()->registeredId;
                }
                return TypeFactory<T, std::integral_constant<decltype(S), S>, std::integral_constant<decltype(G), G>>(type, it.first->second.properties);
            }
        }

        template <auto G, auto N>
        auto MemberNoSetter(const std::string_view &key)
        {
            using Type = std::remove_reference_t<std::invoke_result_t<decltype(G), T &>>;
            if constexpr (std::is_const_v<Type>) {
                auto it    = type.members.emplace(key, serialize::TypeMemberNode{
                        TypeInfoObj<std::remove_const_t<Type>>::Get()->RtInfo(),
                        std::is_const_v<Type>,
                        !std::is_member_object_pointer_v<Type>,
                        nullptr,
                        nullptr,
                        &GetterConst<T, G>,
                        &ValueChanged_<T, N>
                });
                return TypeFactory<T, std::integral_constant<decltype(G), G>>(type, it.first->second.properties);
            } else {
                auto it    = type.members.emplace(key, serialize::TypeMemberNode{
                        TypeInfoObj<std::remove_const_t<Type>>::Get()->RtInfo(),
                        std::is_const_v<Type>,
                        !std::is_member_object_pointer_v<Type>,
                        nullptr,
                        &Getter<T, G>,
                        &GetterConst<T, G>,
                        &ValueChanged_<T, N>
                });
                return TypeFactory<T, std::integral_constant<decltype(G), G>>(type, it.first->second.properties);
            }
        }

        template <auto F>
        auto MemberFunction(const std::string_view &key)
        {
            static_assert(std::is_member_function_pointer_v<decltype(F)>);

            using FuncType = FuncTraits<decltype(F)>;
            using ClsType = typename FuncType::CLASS_TYPE;
            using ArgsType = typename FuncType::ARGS_TYPE;

            type.functions.emplace(key, serialize::MemberFunctionNode {
                std::tuple_size_v<ArgsType>,
                [](Any *args) -> bool { return true; },
                [](void* ptr, Any *args) -> Any {
                    return InvokeMemberFunc<F, ArgsType, ClsType>(static_cast<ClsType*>(ptr), args, std::make_index_sequence<std::tuple_size_v<ArgsType>>{});
                }
            });
        }

        template <typename E>
        TypeFactory &Enum(E val, const std::string_view &name)
        {
            static_assert(std::is_integral_v<std::underlying_type_t<E>>, "T must be integral type");
            type.enums.emplace(static_cast<uint64_t>(val), name);
            return *this;
        }

        template <auto Func>
        auto JsonLoad()
        {
            type.serialization.jsonLoad = SerializationInArchive<Func, JsonInputArchive>();
            return *this;
        }
        template <auto Func>
        auto JsonSave()
        {
            type.serialization.jsonSave = SerializationOutArchive<Func, JsonOutputArchive>();
            return *this;
        }

        template <auto Func>
        auto BinLoad()
        {
            type.serialization.binaryLoad = SerializationInArchive<Func, BinaryInputArchive>();
            return *this;
        }
        template <auto Func>
        auto BinSave()
        {
            type.serialization.binarySave = SerializationOutArchive<Func, BinaryOutputArchive>();
            return *this;
        }

        auto operator()()
        {
            return TypeFactory<T, T>(type, type.properties);
        }

    protected:

        TypeNode &type;
    };

    template <typename T, typename... S>
    class TypeFactory<T, S...> : public TypeFactory<T> {
    public:
        TypeFactory(TypeNode &node, serialize::PropertyMap &pMap) : TypeFactory<T>(node), property(pMap)
        {
        }

        template <typename U>
        TypeFactory &Property(const U &key, const Any &any)
        {
            return Property(static_cast<uint32_t>(key), any);
        }

        TypeFactory &Property(uint32_t key, const Any &any)
        {
            property.emplace(key, any);
            return *this;
        }

    protected:
        serialize::PropertyMap &property;
    };
} // namespace sky
