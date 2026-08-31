//
// Created on 2026/09/01.
//

#pragma once

#include <atomic>
#include <thread>

namespace sky {

    // Lightweight spin lock for short, low-contention critical sections.
    class SpinLock {
    public:
        void lock() noexcept
        {
            while (flag.test_and_set(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        }

        void unlock() noexcept
        {
            flag.clear(std::memory_order_release);
        }

    private:
        std::atomic_flag flag{};
    };

} // namespace sky
