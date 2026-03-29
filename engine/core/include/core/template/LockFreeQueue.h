//
// Created on 2026/03/29.
//

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

namespace sky {

    // Bounded multi-producer multi-consumer lock-free queue.
    // Based on Dmitry Vyukov's bounded MPMC queue algorithm.
    // Capacity is rounded up to the next power of two.
    template <typename T>
    class LockFreeQueue {
    public:
        explicit LockFreeQueue(uint32_t capacity)
            : mask(RoundUpPow2(capacity) - 1)
        {
            const uint32_t size = mask + 1;
            buffer = new Cell[size];
            for (uint32_t i = 0; i < size; ++i) {
                buffer[i].sequence.store(i, std::memory_order_relaxed);
            }
            enqueuePos.store(0, std::memory_order_relaxed);
            dequeuePos.store(0, std::memory_order_relaxed);
        }

        ~LockFreeQueue()
        {
            delete[] buffer;
        }

        LockFreeQueue(const LockFreeQueue &) = delete;
        LockFreeQueue &operator=(const LockFreeQueue &) = delete;

        bool TryPush(T &&item)
        {
            Cell *cell   = nullptr;
            auto  pos    = enqueuePos.load(std::memory_order_relaxed);
            for (;;) {
                cell = &buffer[pos & mask];
                const auto seq  = cell->sequence.load(std::memory_order_acquire);
                const auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
                if (diff == 0) {
                    if (enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        break;
                    }
                } else if (diff < 0) {
                    return false; // full
                } else {
                    pos = enqueuePos.load(std::memory_order_relaxed);
                }
            }
            cell->data = std::move(item);
            cell->sequence.store(pos + 1, std::memory_order_release);
            return true;
        }

        bool TryPop(T &item)
        {
            Cell *cell = nullptr;
            auto  pos  = dequeuePos.load(std::memory_order_relaxed);
            for (;;) {
                cell = &buffer[pos & mask];
                const auto seq  = cell->sequence.load(std::memory_order_acquire);
                const auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
                if (diff == 0) {
                    if (dequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        break;
                    }
                } else if (diff < 0) {
                    return false; // empty
                } else {
                    pos = dequeuePos.load(std::memory_order_relaxed);
                }
            }
            item = std::move(cell->data);
            cell->sequence.store(pos + mask + 1, std::memory_order_release);
            return true;
        }

    private:
        static uint32_t RoundUpPow2(uint32_t v)
        {
            v--;
            v |= v >> 1;
            v |= v >> 2;
            v |= v >> 4;
            v |= v >> 8;
            v |= v >> 16;
            v++;
            return v < 2 ? 2 : v;
        }

        struct Cell {
            std::atomic<size_t> sequence;
            T                   data;
        };

#ifdef __cpp_lib_hardware_interference_size
        static constexpr size_t CacheLineSize = std::hardware_destructive_interference_size;
#else
        static constexpr size_t CacheLineSize = 64;
#endif

        Cell    *buffer;
        uint32_t mask;

        alignas(CacheLineSize) std::atomic<size_t> enqueuePos;
        alignas(CacheLineSize) std::atomic<size_t> dequeuePos;
    };

} // namespace sky
