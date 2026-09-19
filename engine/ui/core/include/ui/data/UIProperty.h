//
// Created on 2026/09/19.
//

#pragma once

#include <cstdint>
#include <string>

namespace sky::ui {

    struct UIPropertyValue {
        enum class Type : uint8_t {
            NONE = 0,
            BOOL,
            INT,
            FLOAT,
            STRING,
        };

        Type type = Type::NONE;
        bool boolValue = false;
        int64_t intValue = 0;
        float floatValue = 0.0f;
        std::string stringValue;

        static UIPropertyValue Bool(bool value);
        static UIPropertyValue Int(int64_t value);
        static UIPropertyValue Float(float value);
        static UIPropertyValue String(const std::string &value);
    };

} // namespace sky::ui
