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
        SettingRegistry()  = default;
        ~SettingRegistry() = default;

        template <typename T>
        void SetValue(std::string_view key, const T &value)
        {
            rapidjson::Pointer pointer(key.data(), key.length());
            if (pointer.IsValid()) {
                if constexpr (std::is_same_v<T, std::string_view>) {
                    rapidjson::Value &setting = pointer.Create(document, document.GetAllocator());
                    setting.SetString(value.data(), static_cast<rapidjson::SizeType>(value.length()), document.GetAllocator());
                } else {
                    pointer.Set(document, value);
                }
            }
        }

        std::string VisitString(std::string_view key) const
        {
            if (document.HasMember(key.data())) {
                auto &value = document[key.data()];
                if (value.IsString()) {
                    return std::string(value.GetString());
                }
            }
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