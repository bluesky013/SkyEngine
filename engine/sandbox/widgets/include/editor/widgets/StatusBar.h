//
// Created on 2026/10/06.
//

#pragma once

#include <ui/UIElement.h>

#include <string>
#include <utility>

namespace sky::ui {
    class UIPaintContext;
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    // Engine-drawn status bar: a single line of resolved editor state.
    class StatusBar : public sky::ui::UIElement {
    public:
        explicit StatusBar(sky::ui::UITextSystem *text);

        const char *GetTypeName() const override { return "StatusBar"; }
        void        SetText(std::string value) { text = std::move(value); }

        void OnPaint(sky::ui::UIPaintContext &context) override;

    private:
        std::string            text;
        sky::ui::UITextSystem *textSystem = nullptr;
    };

} // namespace sky::editor
