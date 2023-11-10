//
// Created by blues on 2023/11/10.
//

#pragma once

#include <cstdint>
#include <list>
#include <vector>

namespace sky::rhi {

    class Allocator {
    public:
        Allocator() = default;
        virtual ~Allocator() = default;

        using Address = uint64_t;

        struct Allocation {
            Address address = 0;
        };

        virtual Allocation *Allocate(uint64_t size, uint64_t alignment) = 0;
    };

    template <uint32_t Size>
    class FixedSizeAllocator : public Allocator {
    public:
        explicit FixedSizeAllocator(uint64_t size) : capacity(size) {}
        ~FixedSizeAllocator() override = default;

        static constexpr uint32_t STRIDE = Size;

        Allocation *Allocate(uint64_t size, uint64_t alignment) override
        {
            return nullptr;
        }

    private:
        uint64_t capacity;
        std::vector<Allocation> allocation;
        std::list<Allocation*> freeList;
    };

} // namespace sky::rhi