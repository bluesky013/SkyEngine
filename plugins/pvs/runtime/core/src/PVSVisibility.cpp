//
// Created on 2026/09/22.
//

#include <pvs/PVSVisibility.h>

namespace sky {

    bool QueryPVSObjectVisible(const uint8_t *data, uint32_t dataSizeInBytes, PVSObjectID id) noexcept
    {
        if (data == nullptr || id >= MAX_OBJECTS) {
            return true;
        }

        const uint32_t byteIndex = id >> 3;
        const uint32_t bitIndex  = id & 7U;
        if (dataSizeInBytes != 0 && byteIndex >= dataSizeInBytes) {
            return true;
        }

        return (data[byteIndex] & (1U << bitIndex)) != 0;
    }

} // namespace sky
