//
// Created by blues on 2023/10/8.
//

#pragma once

#include <type_traits>

namespace sky {

    template <typename T>
    using Result = std::pair<bool, T>;

} // namespace sky