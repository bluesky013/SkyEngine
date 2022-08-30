//
// Created by Zach Lee on 2022/5/29.
//

#pragma once
#include <core/environment/Singleton.h>
#include <core/hash/Crc32.h>
#include <mutex>

namespace sky {

    struct GlobalResourceEmptyKey {};

    template <typename T> struct GlobalResourceTraits {
        using KeyType = GlobalResourceEmptyKey;
    };

    template <typename T> class GlobalResource : public Singleton<GlobalResource<T>> {
    public:
        using ResPtr  = std::shared_ptr<T>;
        using KeyType = typename GlobalResourceTraits<T>::KeyType;

        ResPtr GetResource(const KeyType &value)
        {
            std::lock_guard<std::mutex> lock(mutex);
            uint32_t                    hash = 0;
            if constexpr (std::is_same_v<KeyType, GlobalResourceEmptyKey>) {
                hash = Crc32::Cal(value);
            }

            auto iter = resources.find(hash);
            if (iter != resources.end()) {
                return iter->second;
            }

            auto res = GlobalResourceTraits<T>::Create(value);
            resources.emplace(hash, res);
            return res;
        }

        void FreeAll()
        {
            std::lock_guard<std::mutex> lock(mutex);
            resources.clear();
        }

    private:
        friend class Singleton<GlobalResource<T>>;
        GlobalResource()  = default;
        ~GlobalResource() = default;

        mutable std::mutex                   mutex;
        std::unordered_map<uint32_t, ResPtr> resources;
    };
} // namespace sky
