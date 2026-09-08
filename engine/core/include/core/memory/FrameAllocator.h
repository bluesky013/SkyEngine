//
// Created by Zach Lee on 2026/09/02.
//

#pragma once

#include <core/memory/LinearStorage.h>
#include <core/memory/TransientAllocator.h>

#include <atomic>
#include <cstdint>
#include <new>
#include <type_traits>

namespace sky {

    // ──────────────────────────────────────────────────────────
    // FrameAllocator
    //   Frame-scoped bump allocator facade.
    //   - Non-virtual bump allocation (LinearStorage backed)
    //   - Mark / Rewind checkpoints for phase-based reclamation
    //   - Arena() exposes TransientAllocator for std container adapters
    //   - Atomic statistics: current bytes, peak bytes, allocation count
    //   Single-threaded: caller must not allocate concurrently.
    // ──────────────────────────────────────────────────────────
    class FrameAllocator {
    public:
        explicit FrameAllocator(size_t blockSize = 64 * 1024)
            : mStorage(blockSize)
            , mArena(mStorage)
        {
        }

        ~FrameAllocator() = default;

        FrameAllocator(const FrameAllocator &) = delete;
        FrameAllocator &operator=(const FrameAllocator &) = delete;

        // ---- bump ----
        void *Allocate(size_t size, size_t alignment = alignof(std::max_align_t))
        {
            const size_t newBytes = mCurrentBytes.load() + size;
            mCurrentBytes.store(newBytes);
            if (mPeakBytes.load() < newBytes) {
                mPeakBytes.store(newBytes);
            }
            mAllocationCount.fetch_add(1);
            return mArena.Allocate(size, alignment);
        }

        template <typename T>
        T *AllocateArray(size_t n)
        {
            return static_cast<T *>(Allocate(n * sizeof(T), alignof(T)));
        }

        template <typename T, typename... Args>
        T *Construct(Args &&...args)
        {
            void *ptr = Allocate(sizeof(T), alignof(T));
            return new (ptr) T(std::forward<Args>(args)...);
        }

        // ---- checkpoint / rewind ----
        using Mark = LinearStorage::Mark;

        struct MarkedAllocation {
            Mark mark;
            size_t bytes = 0;
        };

        MarkedAllocation GetMark() const
        {
            return MarkedAllocation{mArena.GetMark(), mCurrentBytes.load()};
        }

        void Rewind(const MarkedAllocation &mark)
        {
            mArena.Rewind(mark.mark);
            mCurrentBytes.store(mark.bytes);
        }

        // ---- std container adapter entry ----
        TransientAllocator &Arena() { return mArena; }
        const TransientAllocator &Arena() const { return mArena; }

        // ---- lifecycle ----
        void Reset()
        {
            mArena.Reset();
            mCurrentBytes.store(0);
            mPeakBytes.store(0);
            mAllocationCount.store(0);
        }

        // ---- statistics ----
        size_t GetCurrentBytes() const { return mCurrentBytes.load(); }
        size_t GetPeakBytes() const { return mPeakBytes.load(); }
        size_t GetAllocationCount() const { return mAllocationCount.load(); }

    private:
        LinearStorage      mStorage;
        TransientAllocator mArena;

        std::atomic<size_t> mCurrentBytes{0};
        std::atomic<size_t> mPeakBytes{0};
        std::atomic<size_t> mAllocationCount{0};
    };

} // namespace sky
