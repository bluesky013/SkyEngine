//
// Created by blues on 2025/10/6.
//

#include <core/archive/BinaryData.h>
#include <core/memory/Allocator.h>
namespace sky {

    BinaryData::BinaryData(uint32_t inSize) : size(inSize)
    {
        if (inSize != 0) {
            rawData = static_cast<uint8_t*>(AlignMalloc(inSize, 16));
        }
    }

    BinaryData::~BinaryData()
    {
        if (rawData != nullptr) {
            AlignFree(rawData);
        }
    }

    void BinaryData::Resize(uint32_t newSize)
    {
        if (size != newSize) {
            void* ptr = AlignMalloc(newSize, 16);
            if (rawData != nullptr) {
                memcpy(ptr, rawData, size);
            }
            rawData = static_cast<uint8_t*>(ptr);
        }

        size = newSize;
    }

} // namespace sky