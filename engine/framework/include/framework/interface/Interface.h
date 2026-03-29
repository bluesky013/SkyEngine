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

        void UnRegister(T &val)
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (ptr == &val) {
                ptr = nullptr;
            }
        }

        void UnRegister()
        {
            std::lock_guard<std::mutex> lock(mutex);
            ptr = nullptr;
        }

        T *GetApi()
        {
            std::lock_guard<std::mutex> lock(mutex);
            return ptr;
        }

        T *operator->()
        {
            return GetApi();
        }

    private:
        friend class Singleton<Interface<T>>;
        Interface()  = default;
        ~Interface() override = default;
        std::mutex mutex;
        T         *ptr = nullptr;
    };

} // namespace sky
