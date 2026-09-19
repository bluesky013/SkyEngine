//
// Created on 2026/09/19.
//

#include <ui/data/UIBuilder.h>
#include <ui/data/UIElementRegistry.h>

namespace sky::ui {

    UIElement *UIBuilder::SetRoot(const std::string &type)
    {
        root = registry.Create(type);
        return root.get();
    }

    UIElement *UIBuilder::AddChild(UIElement *parent, const std::string &type)
    {
        if (parent == nullptr) {
            return nullptr;
        }
        UIElementPtr child = registry.Create(type);
        if (child == nullptr) {
            return nullptr;
        }
        return parent->AddChild(std::move(child));
    }

} // namespace sky::ui
