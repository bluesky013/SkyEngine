//
// Created by Zach Lee on 2021/12/19.
//

#pragma once

#include <atomic>
#include <core/platform/Platform.h>

namespace sky {

    class RefObject {
    public:
        RefObject() = default;
        virtual ~RefObject() = default;

        void AddRef() noexcept
        {
            counter.fetch_add(1);
        }

        void RemoveRef() noexcept
        {
            auto prev = counter.fetch_sub(1);
            SKY_ASSERT(prev >= 1);
            if (prev == 1) {
                OnExpire();
            }
        }

        uint32_t GetRef() const
        {
            return counter.load();
        }

    protected:
        virtual void OnExpire() noexcept
        {
            delete this;
        }

        std::atomic_uint32_t counter;
    };

    template <typename T>
    class CounterPtr {
    public:
        CounterPtr() : ptr(nullptr) {}

        CounterPtr(T *p) // NOLINT
        {
            Reset(p);
        }

        CounterPtr(const CounterPtr &p) : CounterPtr(p.Get())
        {
        }

        CounterPtr(CounterPtr &&p) noexcept
        {
            if (ptr != nullptr) {
                ptr->RemoveRef();
            }
            ptr = p.Release();
        }

        template <typename U>
        CounterPtr(const CounterPtr<U> &p) : CounterPtr(static_cast<T*>(p.Get())) // NOLINT
        {
        }

        CounterPtr &operator=(const CounterPtr &p) noexcept
        {
            Reset(p.ptr);
            return *this;
        }

        template <typename U>
        CounterPtr<T> &operator=(U *p)
        {
            static_assert(std::is_base_of_v<T, U>);
            Reset(static_cast<T*>(p));
            return *this;
        }

        template <typename U>
        CounterPtr<T> &operator=(const CounterPtr<U> &p)
        {
            static_assert(std::is_base_of_v<T, U>);
            Reset(static_cast<T*>(p.Get()));
            return *this;
        }

        template <typename U>
        CounterPtr &operator=(CounterPtr<U> &&p)
        {
            static_assert(std::is_base_of_v<T, U>);
            if (ptr != nullptr) {
                ptr->RemoveRef();
            }
            ptr = static_cast<T*>(p.Release());
            return *this;
        }

        virtual ~CounterPtr()
        {
            if (ptr != nullptr) {
                ptr->RemoveRef();
            }
        }

        void Reset(T *p) noexcept
        {
            if (ptr != nullptr) {
                ptr->RemoveRef();
            }
            ptr = p;
            if (ptr != nullptr) {
                ptr->AddRef();
            }
        }

        T* Release()
        {
            T* ret = ptr;
            ptr = nullptr;
            return ret;
        }

        T *Get() const { return ptr; }
        T &operator*() const { return *ptr; }
        T *operator->() const { return ptr; }
        explicit operator bool() const { return ptr != nullptr; }

    protected:
        template <typename U>
        friend class CounterPtr;

        T *ptr = nullptr;
    };

    template< class T >
    bool operator==(const CounterPtr<T>& p, std::nullptr_t) noexcept
    {
        return p.Get() == nullptr;
    }

    template< class T >
    bool operator!=(const CounterPtr<T>& p, std::nullptr_t) noexcept
    {
        return p.Get() != nullptr;
    }

    template< class T >
    bool operator==(std::nullptr_t, const CounterPtr<T>& p) noexcept
    {
        return p.Get() == nullptr;
    }

    template< class T >
    bool operator!=(std::nullptr_t, const CounterPtr<T>& p) noexcept
    {
        return p.Get() != nullptr;
    }

    template <class Ty1, class Ty2>
    CounterPtr<Ty1> CastPtr(const CounterPtr<Ty2>& ptr) noexcept
    {
        const auto res = static_cast<Ty1*>(ptr.Get());
        return CounterPtr<Ty1>(res);
    }
} // namespace sky