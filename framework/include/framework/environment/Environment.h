//
// Created by Zach Lee on 2021/12/4.
//

#pragma once

#include <unordered_map>
#include <mutex>

namespace sky {

    class Environment {
    public:
        static Environment* Get();

        static void Destroy();

        static void Attach(Environment* env);

        void Register(uint32_t key, void* ptr);

        void UnRegister(uint32_t key);

        void* Find(uint32_t key);

    protected:
        static Environment* instance;

        Environment() = default;
        ~Environment() = default;
        mutable std::mutex mutex;
        std::unordered_map<uint32_t, void*> envMap;
    };

}