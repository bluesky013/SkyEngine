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

        static T* Get(bool release = false)
        {
            static T* instance = nullptr;
            if (instance == nullptr) {
                void* ptr = Environment::Get()->Find(ID);
                if (ptr == nullptr) {
                    instance = new T();
                    Environment::Get()->Register(ID, instance);
                } else {
                    instance = static_cast<T*>(ptr);
                }
            }
            T* res = instance;
            if (release) {
                instance = nullptr;
            }
            return res;
        }

        static void Destroy()
        {
            auto instance = Get(true);
            if (instance != nullptr) {
                Environment::Get()->UnRegister(ID);
                delete instance;
            }
        }
    protected:
        Singleton() = default;
        virtual ~Singleton() = default;
    };

}
