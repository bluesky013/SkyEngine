//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIElement.h>

#include <functional>
#include <string>
#include <unordered_map>

namespace sky::ui {

    class UIElementRegistry {
    public:
        using Factory = std::function<UIElementPtr()>;

        void Register(const std::string &type, Factory factory);
        bool Contains(const std::string &type) const;
        UIElementPtr Create(const std::string &type) const;

        // Seeds the built-in widget types (Panel, Image, Button).
        static UIElementRegistry CreateDefault();

    private:
        std::unordered_map<std::string, Factory> factories;
    };

} // namespace sky::ui
