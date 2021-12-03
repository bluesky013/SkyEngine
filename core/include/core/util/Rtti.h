//
// Created by Zach Lee on 2021/12/3.
//


#pragma once

#ifdef _MSC_VER
#define PRETTY_FUNC __FUNCSIG__
#else
#define PRETTY_FUNC __PRETTY_FUNCTION__
#endif

#include <string_view>
#include <core/hash/Fnv1a.h>

namespace sky {

    template <typename T>
    constexpr std::string_view PrettyFunc()
    {
        return PRETTY_FUNC;
    }

    template <typename T>
    struct TypeInfo {

        static constexpr static std::string_view Name()
        {
            return PrettyFunc<T>();
        }

        static constexpr  uint32_t Hash()
        {
            return Fnv1a32(Name());
        }

    };

}