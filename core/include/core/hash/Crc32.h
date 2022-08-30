//
// Created by Zach Lee on 2022/1/9.
//

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sky {

    class Crc32 {
    public:
        static uint32_t Cal(const uint8_t *buffer, uint32_t size);

        static uint32_t Cal(const std::string &str);

        static uint32_t Cal(const std::string_view &str);

        template <typename T> static uint32_t Cal(const T &t)
        {
            return Cal(reinterpret_cast<const uint8_t *>(&t), sizeof(T));
        }
    };

} // namespace sky