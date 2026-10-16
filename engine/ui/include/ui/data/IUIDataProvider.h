//
// Created on 2026/09/19.
//

#pragma once

#include <ui/data/UIProperty.h>

#include <string>

namespace sky::ui {

    // Core-side seam: resolves a binding source path to a value. The default
    // implementation is UIDataContext; reflection or game view models can replace it.
    class IUIDataProvider {
    public:
        IUIDataProvider() = default;
        virtual ~IUIDataProvider() = default;

        IUIDataProvider(const IUIDataProvider &) = delete;
        IUIDataProvider &operator=(const IUIDataProvider &) = delete;

        virtual bool GetValue(const std::string &path, UIPropertyValue &out) const = 0;

        // Monotonically increasing when values change; 0 means "unknown/always dirty".
        virtual uint32_t GetVersion() const { return 0; }
    };

} // namespace sky::ui
