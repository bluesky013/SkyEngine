//
// Created by Zach Lee on 2022/12/1.
//

#pragma once

#include <type_traits>

namespace sky {

    template <typename T>
    inline T AlignDivision(T lhs, T rhs)
    {
        static_assert(std::is_integral_v<T>, "T must be integral.");
        return (lhs + rhs - 1) / rhs;
    }

}
