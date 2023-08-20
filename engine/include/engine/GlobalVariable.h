//
// Created by Zach Lee on 2022/8/11.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/type/Any.h>
#include <unordered_map>

namespace sky {

    class GlobalVariable : public Singleton<GlobalVariable> {
    public:
        ~GlobalVariable() override = default;

        template <typename T>
        const T *Find(const std::string &key)
        {
            auto iter = valueMap.find(key);
            if (iter != valueMap.end()) {
                return iter->second.GetAs<T>();
            }
            return nullptr;
        }

        template <typename T>
        void Register(const std::string &key, const T &val)
        {
            valueMap[key] = Any(val);
        }

    private:
        friend class Singleton<GlobalVariable>;
        GlobalVariable() = default;

        std::unordered_map<std::string, Any> valueMap;
    };

} // namespace sky