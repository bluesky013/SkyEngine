//
// Created by blues on 2024/2/13.
//

#pragma once

#include <type_traits>

namespace sky {

    template <class T>
    concept ContainerDataType = requires(T a, typename T::value_type b)
    {
        a.size();
        a.data();
        a.resize(1);
    };

    template <class T>
    concept ArithmeticDataType = std::is_arithmetic_v<T> || std::is_arithmetic_v<std::underlying_type_t<T>>;

} // namespace sky