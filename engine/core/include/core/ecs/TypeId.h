//
// ECS type id: explicit tag string -> constexpr FNV-1a hash.
// Types without an explicit SKY_TYPE_TAG fail to compile (no fallback).
//

#pragma once

#include <core/hash/Fnv1a.h>

#include <cstdint>
#include <string_view>

namespace sky {

    // explicit type tag (registered via SKY_TYPE_TAG); default: no value
    template <typename T>
    struct TypeTagOf;

    template <typename T>
    constexpr uint32_t TypeId()
    {
        if constexpr (requires { TypeTagOf<T>::value; }) {
            return Fnv1a32(TypeTagOf<T>::value);
        } else {
            static_assert(sizeof(T) == 0,
                          "TypeId<T>: type has no SKY_TYPE_TAG; register an explicit tag");
            return 0;
        }
    }

    template <typename T>
    constexpr std::string_view TypeTag()
    {
        if constexpr (requires { TypeTagOf<T>::value; }) {
            return TypeTagOf<T>::value;
        } else {
            static_assert(sizeof(T) == 0,
                          "TypeTag<T>: type has no SKY_TYPE_TAG; register an explicit tag");
            return {};
        }
    }

} // namespace sky

// register an explicit, platform/compiler-stable tag for an ECS type.
// convention: "sky.<module>.<TypeName>"
#define SKY_TYPE_TAG(T, STR) \
    template <> struct ::sky::TypeTagOf<T> { static constexpr std::string_view value = STR; };
