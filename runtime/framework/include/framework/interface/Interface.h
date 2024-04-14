//
// Created by Zach Lee on 2022/3/12.
//

#pragma once

#include <core/environment/Singleton.h>
#include <mutex>

namespace sky {

    template <typename T>
    class Interface : public Singleton<Interface<T>> {
    public:
        void Register(T &val)
        {
            std::lock_guard<std::mutex> lock(mutex);
            ptr = &val;
        }

        void UnRegister()
        {
            std::lock_guard<std::mutex> lock(mutex);
            ptr = nullptr;
        }

        T *GetApi()
        {
            return ptr;
        }

        T *operator->()
        {
            return ptr;
        }

    private:
        friend class Singleton<Interface<T>>;
        Interface()  = default;
        ~Interface() = default;
        std::mutex mutex;
        T         *ptr = nullptr;
    };

} // namespace sky