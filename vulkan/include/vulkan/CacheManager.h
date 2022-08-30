//
// Created by Zach Lee on 2022/1/9.
//

#pragma once
#include <functional>
#include <mutex>
#include <unordered_map>

namespace sky::drv {

    class Device;

    template <typename T>
    class CacheManager {
    public:
        CacheManager() = default;

        ~CacheManager()
        {
        }

        void Shutdown()
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (auto &cache : caches) {
                if (deleteFn) {
                    deleteFn(cache.second);
                }
            }
        }

        T Find(uint32_t key)
        {
            std::lock_guard<std::mutex> lock(mutex);
            auto                        iter = caches.find(key);
            if (iter != caches.end()) {
                return iter->second;
            }
            return VK_NULL_HANDLE;
        }

        template <typename CreateFn>
        T FindOrEmplace(uint32_t key, CreateFn &&fn)
        {
            std::lock_guard<std::mutex> lock(mutex);
            auto                        iter = caches.find(key);
            if (iter != caches.end()) {
                return iter->second;
            }

            auto handle = fn();
            if (handle != VK_NULL_HANDLE) {
                caches.emplace(key, handle);
            }
            return handle;
        }

        template <typename DeleteFn>
        void SetUp(DeleteFn &&fn)
        {
            deleteFn = std::move(fn);
        }

    private:
        std::mutex                      mutex;
        std::unordered_map<uint32_t, T> caches;
        std::function<void(T &)>        deleteFn;
    };

} // namespace sky::drv
