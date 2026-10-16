//
// Created on 2026/09/19.
//

#include <ui/data/UIDataContext.h>

#include <utility>

namespace sky::ui {

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
