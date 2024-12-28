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

        static MD5 CalculateMD5(const std::string &input);

        std::string ToString() const;
    };

} // namespace sky
