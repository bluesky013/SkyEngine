//
// Created by Zach Lee on 2022/1/9.
//

#pragma once
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vulkan/vulkan.h>

namespace sky::vk {

    class Device;

    template <typename T, typename Key>
    class CacheManager {
    public:
        CacheManager() = default;
        ~CacheManager() = default;

        using KeyType = Key;

        void Shutdown()
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (auto &cache : caches) {
                if (deleteFn) {
                    deleteFn(cache.second);
                }
            }
        }

        T Find(KeyType key)
        {
            std::lock_guard<std::mutex> lock(mutex);
            auto                        iter = caches.find(key);
            if (iter != caches.end()) {
                return iter->second;
            }
            return VK_NULL_HANDLE;
        }

        template <typename CreateFn>
        T FindOrEmplace(KeyType key, CreateFn &&fn)
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

        template <typename CreateFn>
        const T& FindOrEmplaceRef(KeyType key, CreateFn &&fn)
        {
            std::lock_guard<std::mutex> lock(mutex);
            auto                        iter = caches.find(key);
            if (iter != caches.end()) {
                return iter->second;
            }

            return caches.emplace(key, fn()).first->second;
        }

        template <typename DeleteFn>
        void SetUp(DeleteFn &&fn)
        {
            deleteFn = std::forward<DeleteFn>(fn);
        }

    private:
        std::mutex                      mutex;
        std::unordered_map<KeyType, T> caches;
        std::function<void(T &)>        deleteFn;
    };

} // namespace sky::vk
