//
// Created by Zach Lee on 2021/12/4.
//

#pragma once
#include <cstdint>

namespace sky {

    class Random {
    public:
        Random()  = default;
        ~Random() = default;

        static bool Gen(void *data, uint32_t dataSize);
    };

} // namespace sky