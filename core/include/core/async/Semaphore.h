//
// Created by Zach Lee on 2023/2/2.
//

#pragma once

#include <cstdint>

namespace sky {

    class Semaphore {
    public:
        Semaphore(int initial = 0);
        ~Semaphore();

        void Acquire();
        void Release(uint32_t count = 1);

        void *handle = nullptr;
    };
}