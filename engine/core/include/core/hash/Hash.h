//
// Created by Zach Lee on 2022/1/9.
//

#pragma once
#include <cstdint>
#include <cstddef>
#include <utility>

namespace sky {

    constexpr void HashCombine32(uint32_t &seed, const uint32_t &hash)
    {
        seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    inline uint32_t UnionFloatToU32(float f32)
    {
        union {
            float f32;
            uint32_t u32;
        } res;

        res.f32 = f32;
        return res.u32;
    }

    uint32_t Murmur3Hash32(const uint8_t* data, size_t length, uint32_t seed);
    uint32_t Murmur3Hash32(std::initializer_list<uint32_t> u32List, uint32_t seed);

} // namespace sky
