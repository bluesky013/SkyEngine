//
// Created by blues on 2024/12/29.
//

#pragma once

#include <cstdint>
#include <string>

namespace sky {

    struct MD5 {
        union {
            uint8_t  u8[16];
            uint32_t u32[4];
            uint64_t u64[2];
        };

        bool operator == (const MD5 &val) const;
        bool operator != (const MD5 &val) const;

        static MD5 CalculateMD5(const std::string &input);
        static MD5 CalculateMD5(const char* data, size_t length);

        std::string ToString() const;
    };

} // namespace sky

#include <core/hash/Hash.h>

namespace std {

    template <>
    struct hash<sky::MD5> {
        size_t operator()(const sky::MD5 &md5) const noexcept
        {
            uint32_t val = 0;
            sky::HashCombine32(val, md5.u32[0]);
            sky::HashCombine32(val, md5.u32[1]);
            sky::HashCombine32(val, md5.u32[2]);
            sky::HashCombine32(val, md5.u32[3]);
            return val;
        }
    };

    template <>
    struct equal_to<sky::MD5> {
        bool operator()(const sky::MD5 &x, const sky::MD5 &y) const noexcept
        {
            return x == y;
        }
    };

} // namespace std
