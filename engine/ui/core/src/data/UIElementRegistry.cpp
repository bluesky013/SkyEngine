//
// Created on 2026/09/19.
//

#include <ui/data/UIElementRegistry.h>
#include <ui/widgets/Button.h>
#include <ui/widgets/EditBox.h>
#include <ui/widgets/HBox.h>
#include <ui/widgets/Image.h>
#include <ui/widgets/Panel.h>
#include <ui/widgets/Text.h>
#include <ui/widgets/VBox.h>

#include <utility>

namespace sky::ui {

    void UIElementRegistry::Register(const std::string &type, Factory factory)
    {
        factories[type] = std::move(factory);
    }

    bool UIElementRegistry::Contains(const std::string &type) const
    {
        return factories.find(type) != factories.end();
    }

    UIElementPtr UIElementRegistry::Create(const std::string &type) const
    {
        const auto it = factories.find(type);
        if (it == factories.end()) {
            return nullptr;
        }
        return it->second();
    }

    UIElementRegistry UIElementRegistry::CreateDefault()
    {
        UIElementRegistry registry;
        registry.Register("Panel", []() { return std::make_unique<Panel>(); });
        registry.Register("Image", []() { return std::make_unique<Image>(); });
        registry.Register("Button", []() { return std::make_unique<Button>(); });
        registry.Register("Text", []() { return std::make_unique<Text>(); });
        registry.Register("EditBox", []() { return std::make_unique<EditBox>(); });
        registry.Register("HBox", []() { return std::make_unique<HBox>(); });
        registry.Register("VBox", []() { return std::make_unique<VBox>(); });
        return registry;
    }

} // namespace sky::ui
