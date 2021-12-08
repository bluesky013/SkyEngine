//
// Created by Zach Lee on 2021/12/4.
//

#pragma once

#include <mutex>
#include <core/util/Rtti.h>
#include <framework/environment/Environment.h>

namespace sky {

    template <typename T>
    class Singleton {
    public:
        static constexpr uint32_t NAME = TypeInfo<T>::Name();
        static constexpr uint32_t ID = TypeInfo<T>::Hash();

        static T* Get()
        {
            return GetPtr();
        }

        static void Destroy()
        {
            auto& instance = GetPtr();
            if (instance != nullptr) {
                Environment::Get()->UnRegister(ID);
                delete instance;
                instance = nullptr;
            }
        }
    protected:
        static T*& GetPtr()
        {
            static T* instance = nullptr;
            static std::mutex mutex;
            if (instance == nullptr) {
                std::lock_guard<std::mutex> lock(mutex);
                void* ptr = Environment::Get()->Find(ID);
                if (ptr == nullptr) {
                    instance = new T();
                    Environment::Get()->Register(ID, instance);
                } else {
                    instance = static_cast<T*>(ptr);
                }
            }
            return instance;
        }

        Singleton() = default;
        virtual ~Singleton() = default;
    };

}
