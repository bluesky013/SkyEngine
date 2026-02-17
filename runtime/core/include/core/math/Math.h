//
// Created by Zach Lee on 2021/11/13.
//

#pragma once

#include <cstdint>
#include <cmath>

namespace sky {
#ifndef PI
    static constexpr float PI = 3.1415926f;
#endif

    template <typename T>
    struct VectorTraits
    {
        using BaseType = T;
        static constexpr size_t Size = 1;

        static BaseType Visit(const T& inVal, size_t index) { return inVal; }
    };

}
