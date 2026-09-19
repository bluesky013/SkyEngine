//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIElement.h>

#include <string>

namespace sky::ui {

    class UIElementRegistry;

    // C++ declarative builder that shares the registry/factory layer with the
    // document loader, so code-built trees behave like loaded ones.
    class UIBuilder {
    public:
        explicit UIBuilder(const UIElementRegistry &registry) : registry(registry) {}
        ~UIBuilder() = default;

        UIBuilder(const UIBuilder &) = delete;
        UIBuilder &operator=(const UIBuilder &) = delete;

        UIElement *SetRoot(const std::string &type);
        UIElement *AddChild(UIElement *parent, const std::string &type);

        UIElementPtr Build() { return std::move(root); }

    private:
        const UIElementRegistry &registry;
        UIElementPtr root;
    };

} // namespace sky::ui
