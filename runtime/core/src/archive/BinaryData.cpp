//
// Created by blues on 2025/10/6.
//

#include <core/archive/BinaryData.h>

namespace sky {

    BinaryData::BinaryData(uint32_t size) : data(size)
    {
    }

    BinaryData::~BinaryData()
    {

    }

    void BinaryData::Resize(uint32_t size)
    {
        data.resize(size);
    }

} // namespace sky