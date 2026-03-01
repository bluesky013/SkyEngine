//
// Created by Copilot on 2026/3/1
//
#pragma once

#include <cstdint>
#include <cstddef>
#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
#include <bit>
#endif

namespace sky {

    inline uint32_t CountBits(const uint8_t* ptr, size_t size)
    {
        if (!ptr || size == 0) return 0;
        uint32_t count = 0;
        for (size_t i = 0; i < size; ++i) {
            uint8_t v = ptr[i];
#if defined(_MSC_VER)
            count += static_cast<uint32_t>(__popcnt(v));
#elif defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
            count += static_cast<uint32_t>(std::popcount(static_cast<unsigned int>(v)));
#else
            v = v - ((v >> 1) & 0x55);
            v = (v & 0x33) + ((v >> 2) & 0x33);
            count += (((v + (v >> 4)) & 0x0F));
#endif
        }
        return count;
    }
} // namespace sky

