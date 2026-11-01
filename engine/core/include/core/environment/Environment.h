//
// Created by Zach Lee on 2021/12/4.
//

#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace sky {

    class Environment {
    public:
        static Environment *Get();

        static void Destroy();

        static void Attach(Environment *env);

        static void Detach();

        static bool IsAttached();

        void Register(uint32_t key, void *ptr);

        void *UnRegister(uint32_t key);

        void *Find(uint32_t key);
        void *FindOrCreate(uint32_t key, void *(*create)());

    protected:
        static Environment *instance;
        static std::mutex   globalMutex;
        static bool         attached;

        Environment()  = default;
        ~Environment() = default;
        mutable std::recursive_mutex         mutex;
        std::unordered_map<uint32_t, void *> envMap;
    };

} // namespace sky
