//
// Created by Zach Lee on 2021/12/9.
//

#pragma once
#include <core/type/Rtti.h>
#include <core/type/Container.h>

namespace sky {

    template <typename T>
    const StaticTypeInfo &TypeInfoStatic()
    {
        static const StaticTypeInfo info = {
            std::rank_v<T>,                       // rank
            sizeof(T),                            // size
            std::is_fundamental_v<T>,             // isFundamental
            std::is_void_v<T>,                    // isVoid
            std::is_null_pointer_v<T>,            // isNullptr
            std::is_arithmetic_v<T>,              // isArithmetic
            std::is_floating_point_v<T>,          // isFloatingPoint
            std::is_integral_v<T>,                // isInteger
            std::is_compound_v<T>,                // isCompound
            std::is_pointer_v<T>,                 // isPointer
            std::is_member_pointer_v<T>,          // isMemberObjectPointer
            std::is_member_function_pointer_v<T>, // isMemberFunctionPointer
            std::is_array_v<T>,                   // isArray;
            std::is_enum_v<T>,                    // isEnum;
            std::is_union_v<T>,                   // isUnion;
            std::is_class_v<T>,                   // isClass;
            std::is_trivial_v<T>,                 // isTrivial;
            ContainerTraits<T>::IS_SEQUENCE,      // is sequence container
            false
        };
        return info;
    }

#define IF_CONSTEXPR_FUNDAMENTAL(NAME) if constexpr(std::is_same_v<T, NAME>()) return #NAME;

    template <typename T>
    std::string_view GetArithmeticName()
    {
        static_assert(std::is_arithmetic_v<T>);
        static_assert(!std::is_same_v<T, long>());
        static_assert(!std::is_same_v<T, unsigned long>());

        IF_CONSTEXPR_FUNDAMENTAL(int8_t)
        else IF_CONSTEXPR_FUNDAMENTAL(uint8_t)
        else IF_CONSTEXPR_FUNDAMENTAL(int16_t)
        else IF_CONSTEXPR_FUNDAMENTAL(uint16_t)
        else IF_CONSTEXPR_FUNDAMENTAL(int32_t)
        else IF_CONSTEXPR_FUNDAMENTAL(uint32_t)
        else IF_CONSTEXPR_FUNDAMENTAL(int64_t)
        else IF_CONSTEXPR_FUNDAMENTAL(uint64_t)
        else IF_CONSTEXPR_FUNDAMENTAL(float)
        else IF_CONSTEXPR_FUNDAMENTAL(double)
    }


    template <typename...>
    struct FuncTraits {
        static constexpr bool CONST = false;
    };

    template <typename Ret, typename Cls, typename... Args>
    struct FuncTraits<Ret (Cls::*)(Args...)> {
        using CLASS_TYPE            = Cls;
        using RET_TYPE              = Ret;
        using ARGS_TYPE             = std::tuple<Args...>;
        static constexpr bool CONST = false;
    };

    template <typename Ret, typename Cls, typename... Args>
    struct FuncTraits<Ret (Cls::*)(Args...) const> {
        using CLASS_TYPE            = Cls;
        using RET_TYPE              = Ret;
        using ARGS_TYPE             = std::tuple<Args...>;
        static constexpr bool CONST = true;
    };

    template <typename Ret, typename... Args>
    struct FuncTraits<Ret(*)(Args...)> {
        using RET_TYPE  = Ret;
        using ARGS_TYPE = std::tuple<Args...>;
    };
} // namespace sky
