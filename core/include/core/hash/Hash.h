//
// Created by Zach Lee on 2022/1/9.
//

#pragma once
#include <cstdint>
#include <utility>

namespace sky {

    constexpr void HashCombine32(uint32_t &seed, const uint32_t &hash)
    {
        seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

} // namespace sky
