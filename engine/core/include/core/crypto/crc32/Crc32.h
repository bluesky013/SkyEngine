//
// Created by Copilot on 2025/3/15.
//

#pragma once

#include <cstdint>
#include <cstddef>

namespace sky {

    // CRC32C (Castagnoli) - polynomial 0x1EDC6F41
    uint32_t Crc32C(const uint8_t *data, size_t size);
    uint32_t Crc32C(const uint8_t *data, size_t size, uint32_t crc);

    // CRC32 (IEEE 802.3) - polynomial 0x04C11DB7
    uint32_t CalcCrc32(const uint8_t *data, size_t size);
    uint32_t CalcCrc32(const uint8_t *data, size_t size, uint32_t crc);

} // namespace sky
