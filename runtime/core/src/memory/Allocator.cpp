//
// Created by blues on 2024/4/19.
//

#include <core/memory/Allocator.h>
//#include <mimalloc/mimalloc.h>
#include <algorithm>

namespace sky {

//    class MiMallocAllocator : public IAllocator {
//    public:
//        MiMallocAllocator() = default;
//        ~MiMallocAllocator() override = default;
//
//        void *Allocate(size_t size, size_t alignment) override
//        {
//            alignment = std::max(DEFAULT_ALLOC_ALIGNMENT, alignment);
//            return mi_malloc_aligned(size, alignment);
//        }
//
//        void Deallocate(void *ptr) override
//        {
//            mi_free(ptr);
//        }
//
//    };

    SystemAllocator::SystemAllocator()
//        : impl(std::make_unique<MiMallocAllocator>())
    {
    }

    void *SystemAllocator::Allocate(size_t size, size_t alignment)
    {
        return impl->Allocate(size, alignment);
    }

    void SystemAllocator::Deallocate(void *ptr)
    {
        impl->Deallocate(ptr);
    }
} // namespace sky