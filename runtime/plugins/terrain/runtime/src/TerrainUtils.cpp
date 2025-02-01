//
// Created by blues on 2024/11/26.
//

#include <terrain/TerrainUtils.h>
#include <core/platform/Platform.h>
#include <cstdint>

namespace sky {

    const uint32_t SECTION_SIZE_LUT[] = {
            31,
            63,
            127
    };
    const uint32_t SECTION_SIZE_NUM   = sizeof(SECTION_SIZE_LUT) / sizeof(uint32_t);


    uint32_t ConvertSectionSize(TerrainSectionSize size)
    {
        SKY_ASSERT(static_cast<uint32_t>(size) < SECTION_SIZE_NUM);
        return SECTION_SIZE_LUT[static_cast<uint32_t>(size)];
    }
} // namespace sky