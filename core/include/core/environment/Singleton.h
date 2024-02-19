//
// Created by Zach Lee on 2021/12/4.
//

#pragma once

#include <core/environment/Environment.h>
#include <core/type/Rtti.h>
#include <mutex>

namespace sky {

    template <typename T>
    class Singleton {
    public:
        static constexpr uint32_t         ID   = TypeInfo<T>::Hash();

        static T *Get()
        {
            return GetPtr(true);
        }

        static void Destroy()
        {
            auto &instance = GetPtr(false);
            if (instance != nullptr) {
                Environment::Get()->UnRegister(ID);
                delete instance;
                instance = nullptr;
            }
        }

    protected:
        static T *&GetPtr(bool create)
        {
            static T         *instance = nullptr;
            static std::mutex mutex;
            if (instance == nullptr && create) {
                std::lock_guard<std::mutex> lock(mutex);
                void                       *ptr = Environment::Get()->Find(ID);
                if (ptr == nullptr) {
                    instance = new T();
                    Environment::Get()->Register(ID, instance);
                } else {
                    instance = static_cast<T *>(ptr);
                }
            }
            return instance;
        }

        Singleton()          = default;
        virtual ~Singleton() = default;
    };

} // namespace sky
