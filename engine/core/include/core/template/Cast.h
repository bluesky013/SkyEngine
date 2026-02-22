//
// Created by blues on 2023/12/17.
//

#pragma once

#include <type_traits>

namespace sky {

    template <typename T>
    std::underlying_type_t<T> UCast(const T &val)
    {
        return static_cast<std::underlying_type_t<T>>(val);
    }

} // namespace sky