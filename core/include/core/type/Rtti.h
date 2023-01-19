//
// Created by Zach Lee on 2021/12/3.
//

#pragma once

#ifdef _MSC_VER
    #define PRETTY_FUNC __FUNCSIG__
#else
    #define PRETTY_FUNC __PRETTY_FUNCTION__
#endif

#include <core/hash/Fnv1a.h>
#include <string_view>
#include <type_traits>

namespace sky {

    template <typename T>
    constexpr std::string_view PrettyFunc()
    {
        return PRETTY_FUNC;
    }

    template <typename T>
    struct TypeInfo {

        static constexpr std::string_view Name()
        {
            return PrettyFunc<T>();
        }

        static constexpr uint32_t Hash()
        {
            return Fnv1a32(Name());
        }

        static constexpr T *Allocate()
        {
            return new T();
        }

        static constexpr void Free(T *ptr)
        {
            ptr->~T();
        }
    };

    using Destructor  = void (*)(void *ptr);
    using Constructor = void (*)(void *ptr);
    using CopyFn      = void (*)(const void *src, void *dst);

    struct TypeInfoRT {
        std::string_view       markedName;
        const std::string_view signature;
        const uint32_t         typeId;
        const size_t           rank;
        const size_t           size;
        const bool             isFundamental;
        const bool             isVoid;
        const bool             isNullptr;
        const bool             isArithmetic;
        const bool             isFloatingPoint;
        const bool             isInteger;
        const bool             isCompound;
        const bool             isPointer;
        const bool             isMemberObjectPointer;
        const bool             isMemberFunctionPointer;
        const bool             isArray;
        const bool             isEnum;
        const bool             isUnion;
        const bool             isClass;
        const bool             isTrivial;
        Constructor            constructor = nullptr;
        Destructor             destructor  = nullptr;
        CopyFn                 copy        = nullptr;
    };

    template <typename T>
    struct TypeAllocate {
        static constexpr bool CTOR = std::is_default_constructible_v<T>;
        static constexpr bool DTOR = std::is_destructible_v<T>;
        static constexpr bool COPY = std::is_copy_constructible_v<T>;

        static void Construct(void *ptr)
        {
            if constexpr (CTOR) {
                new (ptr) T{};
            }
        }

        static void Destruct(void *ptr)
        {
            if constexpr (DTOR) {
                ((T *)ptr)->~T();
            }
        }

        static void Copy(const void *src, void *dst)
        {
            if constexpr (COPY) {
                new (dst) T{*((T *)src)};
            }
        }
    };

} // namespace sky
