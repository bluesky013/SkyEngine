//
// Created by Zach Lee on 2022/1/13.
//

#pragma once

#include <cstdint>
#ifdef _MSC_VER
    #include <intrin.h>
#endif

namespace sky {

    template <typename T>
    inline T Align(T size, T alignment)
    {
        return ((size + alignment - 1) & (~(alignment - 1)));
    }

    inline uint8_t BitScan(uint32_t bitMap)
    {
#ifdef _MSC_VER
        unsigned long pos;
        if (_BitScanForward(&pos, bitMap)) {
            return static_cast<uint8_t>(pos);
        }
        return UINT8_MAX;
#elif defined __GNUC__ || defined __clang__
        return static_cast<uint8_t>(__builtin_ffs(bitMap)) - 1U;
#else
        uint8_t pos = 0;
        uint32_t bit = 1;
        do
        {
            if (bitMap & bit)
                return pos;
            bit <<= 1;
        } while (pos++ < 31);
        return UINT8_MAX;
#endif
    }

    inline uint8_t BitScanReverse(uint32_t bitMap)
    {
#ifdef _MSC_VER
        unsigned long pos;
        if (_BitScanReverse(&pos, bitMap)) {
            return static_cast<uint8_t>(pos);
        }
#elif defined __GNUC__ || defined __clang__
        if (bitMap)
            return 31 - static_cast<uint8_t>(__builtin_clz(bitMap));
#else
        uint8_t pos = 31;
        uint32_t bit = 1UL << 31;
        do
        {
            if (bitMap & bit)
                return pos;
            bit >>= 1;
        } while (pos-- > 0);
#endif
        return UINT8_MAX;
    }

    inline uint8_t BitScan(uint64_t bitMap)
    {
#ifdef _MSC_VER
        unsigned long pos;
        if (_BitScanForward64(&pos, bitMap)) {
            return static_cast<uint8_t>(pos);
        }
        return UINT8_MAX;
#elif defined __GNUC__ || defined __clang__
        return static_cast<uint8_t>(__builtin_ffs(bitMap)) - 1U;
#else
        uint8_t pos = 0;
        uint64_t bit = 1;
        do
        {
            if (bitMap & bit)
                return pos;
            bit <<= 1;
        } while (pos++ < 63);
        return UINT8_MAX;
#endif
    }

    inline uint8_t BitScanReverse(uint64_t bitMap)
    {
#ifdef _MSC_VER
        unsigned long pos;
        if (_BitScanReverse64(&pos, bitMap)) {
            return static_cast<uint8_t>(pos);
        }
#elif defined __GNUC__ || defined __clang__
        if (bitMap) {
            return 63 - static_cast<uint8_t>(__builtin_clz(bitMap));
        }
#else
        uint8_t pos = 63;
        uint64_t bit = 1UL << 63;
        do
        {
            if (bitMap & bit)
                return pos;
            bit >>= 1;
        } while (pos-- > 0);
#endif
        return UINT8_MAX;
    }
} // namespace sky
