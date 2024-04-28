//
// Created by blues on 2024/4/6.
//

#pragma once

#include <core/memory/LinkedStorage.h>
#include <core/memory/LinearStorage.h>
#include <core/environment/Singleton.h>
#include <memory>

namespace sky {
    static constexpr size_t DEFAULT_ALLOC_ALIGNMENT = 8;

    class IAllocator {
    public:
        IAllocator() = default;
        virtual ~IAllocator() = default;

        virtual void *Allocate(size_t size, size_t alignment) = 0;
        virtual void Deallocate(void *ptr) = 0;
    };

    class SystemAllocator : public Singleton<SystemAllocator> {
    public:
        SystemAllocator();
        ~SystemAllocator() override = default;

        void *Allocate(size_t size, size_t alignment);
        void Deallocate(void *ptr);

    private:
        std::unique_ptr<IAllocator> impl;
    };


#define SKY_ALLOCATOR(Name, Allocator)                           \
    inline void* operator new(size_t size)                       \
    {                                                            \
        return Allocator::Get()->Allocate(size, alignof(Name)); \
    }                                                            \
    inline void operator delete(void* ptr, size_t size)          \
    {                                                            \
        if (ptr) Allocator::Get()->Deallocate(ptr);              \
    }
} // namespace sky