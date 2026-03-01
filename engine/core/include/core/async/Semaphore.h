//
// Created by Zach Lee on 2023/2/2.
//

#pragma once

#include <cstdint>

namespace sky {

    class Semaphore {
    public:
        explicit Semaphore(int initial = 0);
        ~Semaphore();

        void Wait();
        void Signal(int32_t count = 1);

        union {
            void *handle;
            uint32_t uHandle;
        };
    };
}
