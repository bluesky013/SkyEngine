//
// Created on 2026/09/01.
//

#pragma once

#include <cstdint>
#include <new>
#include <utility>

namespace sky {

    // Vector with N inline elements. Avoids a heap allocation until the
    // element count exceeds N, then spills to a heap buffer.
    template <typename T, uint32_t N>
    class SmallVector {
    public:
        using value_type = T;

        SmallVector() = default;

        ~SmallVector()
        {
            Clear();
        }

        SmallVector(SmallVector &&other) noexcept
        {
            MoveFrom(std::move(other));
        }

        SmallVector &operator=(SmallVector &&other) noexcept
        {
            if (this != &other) {
                Clear();
                MoveFrom(std::move(other));
            }
            return *this;
        }

        SmallVector(const SmallVector &)            = delete;
        SmallVector &operator=(const SmallVector &) = delete;

        void push_back(const T &value)
        {
            EnsureCapacity(count + 1);
            new (Element(count)) T(value);
            ++count;
        }

        void push_back(T &&value)
        {
            EnsureCapacity(count + 1);
            new (Element(count)) T(std::move(value));
            ++count;
        }

        uint32_t size() const
        {
            return count;
        }
        bool empty() const
        {
            return count == 0;
        }

        T *begin()
        {
            return Element(0);
        }
        T *end()
        {
            return Element(count);
        }
        const T *begin() const
        {
            return Element(0);
        }
        const T *end() const
        {
            return Element(count);
        }

    private:
        T *Element(uint32_t i)
        {
            return heap != nullptr ? heap + i : reinterpret_cast<T *>(inlineBuffer + i * sizeof(T));
        }

        const T *Element(uint32_t i) const
        {
            return heap != nullptr ? heap + i : reinterpret_cast<const T *>(inlineBuffer + i * sizeof(T));
        }

        void EnsureCapacity(uint32_t required)
        {
            const uint32_t currentCap = heap != nullptr ? heapCapacity : N;
            if (required <= currentCap) {
                return;
            }

            uint32_t newCap = currentCap * 2;
            if (newCap < required) {
                newCap = required;
            }

            T *newHeap = static_cast<T *>(::operator new(newCap * sizeof(T)));
            for (uint32_t i = 0; i < count; ++i) {
                new (newHeap + i) T(std::move(*Element(i)));
                Element(i)->~T();
            }
            heap         = newHeap;
            heapCapacity = newCap;
        }

        void Clear()
        {
            for (uint32_t i = 0; i < count; ++i) {
                Element(i)->~T();
            }
            if (heap != nullptr) {
                ::operator delete(heap);
                heap         = nullptr;
                heapCapacity = 0;
            }
            count = 0;
        }

        void MoveFrom(SmallVector &&other)
        {
            if (other.heap != nullptr) {
                heap               = other.heap;
                heapCapacity       = other.heapCapacity;
                count              = other.count;
                other.heap         = nullptr;
                other.heapCapacity = 0;
                other.count        = 0;
            } else {
                for (uint32_t i = 0; i < other.count; ++i) {
                    new (Element(i)) T(std::move(*other.Element(i)));
                    other.Element(i)->~T();
                }
                count       = other.count;
                other.count = 0;
            }
        }

        alignas(T) uint8_t inlineBuffer[N * sizeof(T)]{};
        T       *heap         = nullptr;
        uint32_t heapCapacity = 0;
        uint32_t count        = 0;
    };

} // namespace sky
