//
// Created on 2026/10/06.
//

#include <editor/core/preferences/PreferenceTypes.h>

#include <utility>

namespace sky::editor {

    PreferenceValue PreferenceValue::Bool(bool value)
    {
        PreferenceValue result;
        result.type      = PreferenceType::BOOL;
        result.boolValue = value;
        return result;
    }

    PreferenceValue PreferenceValue::Int(int64_t value)
    {
        PreferenceValue result;
        result.type     = PreferenceType::INT;
        result.intValue = value;
        return result;
    }

    PreferenceValue PreferenceValue::Float(double value)
    {
        PreferenceValue result;
        result.type       = PreferenceType::FLOAT;
        result.floatValue = value;
        return result;
    }

    PreferenceValue PreferenceValue::Str(std::string value)
    {
        PreferenceValue result;
        result.type        = PreferenceType::STRING;
        result.stringValue = std::move(value);
        return result;
    }

    PreferenceValue PreferenceValue::Color(const PreferenceColor &value)
    {
        PreferenceValue result;
        result.type       = PreferenceType::COLOR;
        result.colorValue = value;
        return result;
    }

    bool PreferenceValue::Equals(const PreferenceValue &other) const
    {
        if (type != other.type) {
            return false;
        }
        switch (type) {
        case PreferenceType::BOOL: return boolValue == other.boolValue;
        case PreferenceType::INT: return intValue == other.intValue;
        case PreferenceType::FLOAT: return floatValue == other.floatValue;
        case PreferenceType::STRING: return stringValue == other.stringValue;
        case PreferenceType::COLOR:
            return colorValue.r == other.colorValue.r && colorValue.g == other.colorValue.g && colorValue.b == other.colorValue.b &&
                   colorValue.a == other.colorValue.a;
        }
        return false;
    }

} // namespace sky::editor
