//
// Created by Zach Lee on 2022/1/9.
//

#include <core/hash/Crc32.h>
#include <crc32c/crc32c.h>

namespace sky {

    uint32_t Crc32::Cal(const uint8_t* buffer, uint32_t size)
    {
        return crc32c::Crc32c(buffer, size);
    }

    uint32_t Crc32::Cal(const std::string& str)
    {
        return crc32c::Crc32c(str);
    }

    uint32_t Crc32::Cal(const std::string_view& str)
    {
        return crc32c::Crc32c(str.data(), str.length());
    }

}