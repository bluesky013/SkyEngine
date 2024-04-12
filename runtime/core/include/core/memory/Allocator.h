//
// Created by blues on 2024/4/6.
//

#pragma once

#include <core/memory/LinkedStorage.h>
#include <core/memory/LinearStorage.h>

namespace sky {

    class IAllocator {
    public:
        IAllocator() = default;
        virtual ~IAllocator() = default;

        virtual uint8_t *Allocate(uint32_t size, uint32_t alignment) = 0;
    };

} // namespace sky