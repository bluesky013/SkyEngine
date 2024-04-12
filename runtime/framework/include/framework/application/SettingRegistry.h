//
// Created by Zach Lee on 2022/3/13.
//

#pragma once

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/rapidjson.h>
#include <string>
#include <string_view>

namespace sky {

    class SettingRegistry {
    public:
        SettingRegistry() { document.SetObject(); }
        ~SettingRegistry() = default;

        template <typename T>
        void SetValue(std::string_view key, const T &value)
        {
            rapidjson::Value kValue(key.data(), static_cast<rapidjson::SizeType>(key.length()), document.GetAllocator());
            rapidjson::Value rValue;
            if constexpr (std::is_same_v<T, std::string_view> || std::is_same_v<T, std::string>) {
                rValue.SetString(value.data(), document.GetAllocator());
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                rValue.SetUint64(value);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                rValue.SetInt64(value);
            } else if constexpr (std::is_floating_point_v<T>) {
                rValue.SetDouble(value);
            } else if constexpr (std::is_signed_v<T>) {
                rValue.SetInt(static_cast<int32_t>(value));
            } else if constexpr (std::is_unsigned_v<T>) {
                rValue.SetUint(static_cast<uint32_t>(value));
            }
            document.AddMember(kValue, rValue, document.GetAllocator());
        }

        std::string VisitString(std::string_view key) const
        {
            if (document.HasMember(key.data())) {
                auto &value = document[key.data()];
                if (value.IsString()) {
                    return std::string(value.GetString());
                }
            }
            auto member = document.MemberCount();
            return {};
        }

        template <class Func>
        void VisitStringArray(std::string_view key, Func &&func) const
        {
            if (document.HasMember(key.data())) {
                auto &value = document[key.data()];
                if (value.IsArray()) {
                    auto array = value.GetArray();
                    for (auto &val : array) {
                        if (val.IsString()) {
                            func(val.GetString());
                        }
                    }
                }
            }
        }

        void Swap(SettingRegistry &registry);

        void Save(std::string &out) const;

    private:
        rapidjson::Document document;
    };

} // namespace sky
