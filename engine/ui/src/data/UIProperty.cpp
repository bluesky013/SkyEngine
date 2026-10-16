// UI data-value construction shared by UI providers and implementations.

#include <ui/data/UIProperty.h>

namespace sky::ui {

    UIPropertyValue UIPropertyValue::Bool(bool value)
    {
        UIPropertyValue result;
        result.type = Type::BOOL;
        result.boolValue = value;
        return result;
    }

    UIPropertyValue UIPropertyValue::Int(int64_t value)
    {
        UIPropertyValue result;
        result.type = Type::INT;
        result.intValue = value;
        return result;
    }

    UIPropertyValue UIPropertyValue::Float(float value)
    {
        UIPropertyValue result;
        result.type = Type::FLOAT;
        result.floatValue = value;
        return result;
    }

    UIPropertyValue UIPropertyValue::String(const std::string &value)
    {
        UIPropertyValue result;
        result.type = Type::STRING;
        result.stringValue = value;
        return result;
    }

} // namespace sky::ui
