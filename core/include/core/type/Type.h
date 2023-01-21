//
// Created by Zach Lee on 2021/12/9.
//

#pragma once
#include <core/environment/Singleton.h>
#include <core/type/Rtti.h>

namespace sky {

    template <typename T>
    class TypeInfoObj : public Singleton<TypeInfoObj<T>> {
    public:
        TypeInfoRT *RtInfo()
        {
            if (info == nullptr) {
                std::lock_guard<std::mutex> lock(mutex);
                if (info == nullptr) {
                    info = new TypeInfoRT{
                        "",                                   // name
                        TypeInfo<T>::Name(),                  // typeId
                        TypeInfo<T>::Hash(),                  // hash
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
                        TypeAllocate<T>::CTOR ? &TypeAllocate<T>::Construct : nullptr,
                        TypeAllocate<T>::DTOR ? &TypeAllocate<T>::Destruct : nullptr,
                        TypeAllocate<T>::COPY ? &TypeAllocate<T>::Copy : nullptr
                    };
                }
            }
            return info;
        }

    private:
        std::mutex  mutex;
        TypeInfoRT *info = nullptr;
    };

    template <typename...>
    struct FuncTraits;

    template <typename Ret, typename Cls, typename... Args>
    struct FuncTraits<Ret (Cls::*)(Args...)> {
        using CLASS_TYPE            = Cls;
        using RET_TYPE              = Ret;
        using ARGS_TYPE             = std::tuple<Cls *, Args...>;
        static constexpr bool CONST = false;
    };

    template <typename Ret, typename Cls, typename... Args>
    struct FuncTraits<Ret (Cls::*)(Args...) const> {
        using CLASS_TYPE            = Cls;
        using RET_TYPE              = Ret;
        using ARGS_TYPE             = std::tuple<const Cls *, Args...>;
        static constexpr bool CONST = true;
    };

    template <typename Ret, typename... Args>
    struct FuncTraits<Ret(*)(Args...)> {
        using RET_TYPE  = Ret;
        using ARGS_TYPE = std::tuple<Args...>;
    };

} // namespace sky

#define TYPE_RTTI(name)                                                                                                                              \
    static const char *TypeName()                                                                                                                    \
    {                                                                                                                                                \
        return #name;                                                                                                                                \
    }
