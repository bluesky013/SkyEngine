//
// Created on 2026/09/19.
//

#pragma once

#include <ui/data/IUIDataProvider.h>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace sky::ui {

    class UIDataContext : public IUIDataProvider {
    public:
        UIDataContext() = default;
        ~UIDataContext() override = default;

        void SetValue(const std::string &path, const UIPropertyValue &value);
        bool GetValue(const std::string &path, UIPropertyValue &out) const override;
        bool HasValue(const std::string &path) const;

        void Clear();
        uint32_t GetVersion() const override { return version; }

    private:
        std::unordered_map<std::string, UIPropertyValue> values;
        uint32_t version = 0;
    };

} // namespace sky::ui
