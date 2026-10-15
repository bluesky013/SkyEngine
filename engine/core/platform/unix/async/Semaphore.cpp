//
// Created by Zach Lee on 2023/6/1.
//

#include <core/async/Semaphore.h>

#include <climits>
#include <semaphore>
#include <stdexcept>

namespace sky {

    using NativeSemaphore = std::counting_semaphore<INT_MAX>;

    Semaphore::Semaphore(int initial)
    {
        if (initial < 0) {
            throw std::invalid_argument("Semaphore count must be nonnegative");
        }
        handle = new NativeSemaphore(initial);
    }

    Semaphore::~Semaphore()
    {
        delete static_cast<NativeSemaphore *>(handle);
    }

    void Semaphore::Wait()
    {
        static_cast<NativeSemaphore *>(handle)->acquire();
    }

    void Semaphore::Signal(int32_t count)
    {
        if (count > 0) {
            static_cast<NativeSemaphore *>(handle)->release(count);
        }
    }
} // namespace sky
