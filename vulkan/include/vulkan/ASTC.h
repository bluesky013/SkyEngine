//
// Created by Zach Lee on 2022/10/23.
//

#pragma once

#include <cstdint>

namespace sky::vk {

    struct ASTCHeader {
        uint8_t magic[4];
        uint8_t blockX;
        uint8_t blockY;
        uint8_t blockZ;
        uint8_t dim_x[3];
        uint8_t dim_y[3];
        uint8_t dim_z[3];
    };

    uint32_t ASTCDim(uint8_t dim[3])
    {
        return dim[0] + (dim[1] << 8) + (dim[2] << 16);
    }

}
