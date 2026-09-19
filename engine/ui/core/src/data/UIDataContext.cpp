//
// Created on 2026/09/19.
//

#include <ui/data/UIDataContext.h>

#include <utility>

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

    void UIDataContext::SetValue(const std::string &path, const UIPropertyValue &value)
    {
        values[path] = value;
        version++;
    }

    bool UIDataContext::GetValue(const std::string &path, UIPropertyValue &out) const
    {
        const auto it = values.find(path);
        if (it == values.end()) {
            return false;
        }
        out = it->second;
        return true;
    }

    bool UIDataContext::HasValue(const std::string &path) const
    {
        return values.find(path) != values.end();
    }

    void UIDataContext::Clear()
    {
        values.clear();
        version++;
    }

} // namespace sky::ui
