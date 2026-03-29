//
// Created by Zach Lee on 2022/1/9.
//

#include <core/hash/Crc32.h>
#include <core/crypto/crc32/Crc32.h>

namespace sky {

    uint32_t Crc32::Cal(const uint8_t *buffer, uint32_t size)
    {
        return Crc32C(buffer, size);
    }

    uint32_t Crc32::Cal(const std::string &str)
    {
        return Crc32C(reinterpret_cast<const uint8_t *>(str.data()), str.size());
    }

    uint32_t Crc32::Cal(const std::string_view &str)
    {
        return Crc32C(reinterpret_cast<const uint8_t *>(str.data()), str.size());
    }

} // namespace sky