//
// Created by Zach Lee on 2021/12/4.
//

#include <framework/environment/Environment.h>

namespace sky {

    Environment* Environment::instance = nullptr;

    Environment* Environment::Get()
    {
        if (instance == nullptr) {
            instance = new Environment();
        }
        return instance;
    }

    void Environment::Destroy()
    {
        if (instance != nullptr) {
            delete instance;
            instance = nullptr;
        }
    }

    void Environment::Attach(Environment* env)
    {
        instance = env;
    }

    void Environment::Register(uint32_t key, void* ptr)
    {
        if (ptr == nullptr) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex);
        envMap.emplace(key, ptr);
    }

    void Environment::UnRegister(uint32_t key)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto iter = envMap.find(key);
        if (iter != envMap.end()) {
            envMap.erase(iter);
        }
    }

    void* Environment::Find(uint32_t key)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto iter = envMap.find(key);
        if (iter != envMap.end()) {
            return iter->second;
        }
        return nullptr;
    }
}