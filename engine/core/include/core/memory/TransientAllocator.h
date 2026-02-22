//
// Created by blues on 2026/2/20.
//

#pragma once

#include <core/memory/LinearStorage.h>
#include <limits>
#include <type_traits>
#include <vector>
#include <list>
#include <string>
#include <unordered_map>

namespace sky {

    // ──────────────────────────────────────────────────────────
    // TransientAllocator
    //   A frame-scoped bump allocator backed by LinearStorage.
    //   Call Reset() once per frame to rewind (no per-object free).
    //   Supports expansion: if the current block is full, a new
    //   block of the same size is allocated transparently.
    // ──────────────────────────────────────────────────────────
    class TransientAllocator {
    public:
        explicit TransientAllocator(size_t blockSize = 64 * 1024)
            : storage(blockSize) {}

        explicit TransientAllocator(LinearStorage &external)
            : storage(1)
            , externalStorage(&external) {}

        ~TransientAllocator() = default;

        TransientAllocator(const TransientAllocator &) = delete;
        TransientAllocator &operator=(const TransientAllocator &) = delete;

        void *Allocate(size_t size, size_t alignment = alignof(std::max_align_t))
        {
            return GetStorage().Allocate(size, alignment);
        }

        // Deallocate is intentionally a no-op.
        // Memory is reclaimed in bulk via Reset().
        void Deallocate(void * /*ptr*/, size_t /*size*/) noexcept {}

        void Reset() { GetStorage().Reset(); }
        void Shrink() { GetStorage().Shrink(); }

        size_t GetCurrentUsedSize() const { return GetStorage().GetCurrentUsedSize(); }

        LinearStorage &GetStorage() { return externalStorage ? *externalStorage : storage; }
        const LinearStorage &GetStorage() const { return externalStorage ? *externalStorage : storage; }

    private:
        LinearStorage  storage;
        LinearStorage *externalStorage = nullptr;
    };

    // ──────────────────────────────────────────────────────────
    // TransientStdAllocator<T>
    //   A C++17 conforming allocator that delegates to a
    //   TransientAllocator. Usable with std::vector, std::string,
    //   std::unordered_map, etc.
    //
    //   IMPORTANT: The underlying TransientAllocator must outlive
    //   all containers using this allocator. Calling Reset() on the
    //   TransientAllocator invalidates ALL memory — only do this
    //   after all containers are destroyed or no longer accessed.
    // ──────────────────────────────────────────────────────────
    template <typename T>
    class TransientStdAllocator {
    public:
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using propagate_on_container_move_assignment = std::true_type;
        using is_always_equal = std::false_type;

        explicit TransientStdAllocator(TransientAllocator &alloc) noexcept
            : allocator(&alloc) {}

        template <typename U>
        TransientStdAllocator(const TransientStdAllocator<U> &other) noexcept // NOLINT
            : allocator(other.GetAllocator()) {}

        T *allocate(size_type n)
        {
            if (n > std::numeric_limits<size_type>::max() / sizeof(T)) {
                throw std::bad_alloc();
            }
            auto *ptr = static_cast<T *>(allocator->Allocate(n * sizeof(T), alignof(T)));
            if (!ptr) {
                throw std::bad_alloc();
            }
            return ptr;
        }

        void deallocate(T *ptr, size_type n) noexcept
        {
            allocator->Deallocate(ptr, n * sizeof(T));
        }

        TransientAllocator *GetAllocator() const noexcept { return allocator; }

    private:
        TransientAllocator *allocator;
    };

    template <typename T, typename U>
    bool operator==(const TransientStdAllocator<T> &a, const TransientStdAllocator<U> &b) noexcept
    {
        return a.GetAllocator() == b.GetAllocator();
    }

    template <typename T, typename U>
    bool operator!=(const TransientStdAllocator<T> &a, const TransientStdAllocator<U> &b) noexcept
    {
        return !(a == b);
    }

    // ──────────────────────────────────────────────────────────
    // Convenience type aliases for transient containers
    // ──────────────────────────────────────────────────────────
    template <typename T>
    using TransientList = std::list<T, TransientStdAllocator<T>>;

    using TransientString = std::basic_string<char, std::char_traits<char>, TransientStdAllocator<char>>;

    template <typename K, typename V, typename Hash = std::hash<K>, typename KeyEq = std::equal_to<K>>
    using TransientHashMap = std::unordered_map<K, V, Hash, KeyEq,
        TransientStdAllocator<std::pair<const K, V>>>;

    // ──────────────────────────────────────────────────────────
    // Factory helpers — shorter than brace-init with explicit allocator
    //   auto vec = MakeTransientVector<int>(alloc);
    //   auto str = MakeTransientString(alloc);
    // ──────────────────────────────────────────────────────────
    template <typename T>
    TransientList<T> MakeTransientList(TransientAllocator &alloc)
    {
        return TransientList<T>{TransientStdAllocator<T>{alloc}};
    }

    inline TransientString MakeTransientString(TransientAllocator &alloc)
    {
        return TransientString{TransientStdAllocator<char>{alloc}};
    }

    template <typename K, typename V, typename Hash = std::hash<K>, typename KeyEq = std::equal_to<K>>
    TransientHashMap<K, V, Hash, KeyEq> MakeTransientHashMap(TransientAllocator &alloc)
    {
        return TransientHashMap<K, V, Hash, KeyEq>{
            0, Hash{}, KeyEq{},
            TransientStdAllocator<std::pair<const K, V>>{alloc}};
    }

} // namespace sky

