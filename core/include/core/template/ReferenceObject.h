//
// Created by Zach Lee on 2021/12/19.
//

#pragma once

#include <atomic>
#include <core/platform/Platform.h>

namespace sky {

    template<typename T>
    class RefObject {
    public:
        using ObjType = T;

        RefObject() : counter{} {}

        virtual ~RefObject() {}

        virtual void AddRef()
        {
            counter.fetch_add(1);
        }

        virtual void RemoveRef()
        {
            auto prev = counter.fetch_sub(1);
            SKY_ASSERT(prev >= 1);
            if (prev == 1) {
                OnExpire();
            }
        }

        virtual void OnExpire()
        {
        }

        uint32_t GetRef() const
        {
            return counter.load();
        }

    private:
        std::atomic_uint32_t counter;
    };

    template<typename T>
    class CounterPtrBase {
    public:
        CounterPtrBase(T *p) : ptr(p)
        {
            if (ptr != nullptr) {
                ptr->AddRef();
            }
        }

        virtual ~CounterPtrBase()
        {
            if (ptr != nullptr) {
                ptr->RemoveRef();
            }
        }

        CounterPtrBase(const CounterPtrBase &p) { UpdateRefs(p); }

        CounterPtrBase &operator=(const CounterPtrBase &p)
        {
            UpdateRefs(p);
            return *this;
        }

        T *Get()
        {
            return ptr;
        }

    protected:
        void UpdateRefs(const CounterPtrBase &p)
        {
            if (ptr != nullptr) {
                ptr->RemoveRef();
            }
            ptr = p.ptr;
            if (ptr != nullptr) {
                ptr->AddRefs();
            }
        }

        T *ptr;
    };

    template<typename T>
    class CounterPtr : public CounterPtrBase<typename T::ObjType> {
    public:
        CounterPtr() : CounterPtrBase<typename T::ObjType>(nullptr) {}

        template<typename U>
        CounterPtr(U *p) : CounterPtrBase<typename T::ObjType>(p) {}

        template<typename U>
        CounterPtr(CounterPtr<U> &u) : CounterPtrBase<typename T::ObjType>(u.Get())
        {
            static_assert(std::is_base_of_v<T, U>, "U must derived from T");
        }

        template<typename U>
        CounterPtr &operator=(const CounterPtr<U> &u)
        {
            static_assert(std::is_base_of_v<T, U>, "U must derived from T");
            CounterPtrBase<typename T::ObjType>::operator=(u);
            return *this;
        }

        ~CounterPtr() {}

        operator bool() { return CounterPtrBase<typename T::ObjType>::ptr != nullptr; }

        T *operator->() { return static_cast<T *>(CounterPtrBase<typename T::ObjType>::ptr); }
    };

}