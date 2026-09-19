//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIElement.h>

#include <cstddef>
#include <string>

namespace sky::ui {

    class UITextSystem;

    // Focusable single-line text field with a caret.
    class EditBox : public UIElement {
    public:
        EditBox();
        ~EditBox() override = default;

        const char *GetTypeName() const override { return "EditBox"; }

        void SetText(const std::string &value);
        const std::string &GetText() const { return text; }

        void SetCaret(size_t value);
        size_t GetCaret() const { return caret; }

        void SetTextSystem(UITextSystem *value);
        UITextSystem *GetTextSystem() const { return textSystem; }

        void SetFontSize(uint32_t value);
        uint32_t GetFontSize() const { return fontSize; }

        void SetTextColor(uint32_t value);
        uint32_t GetTextColor() const { return textColor; }

        UIEventResult OnPointerEvent(const UIPointerEvent &event) override;
        UIEventResult OnKeyEvent(const UIKeyEvent &event) override;
        UIEventResult OnTextInput(const UITextInputEvent &event) override;

        void Measure(float &outWidth, float &outHeight) override;
        void OnPaint(UIPaintContext &context) override;
        bool SetProperty(const std::string &path, const UIPropertyValue &value) override;

    private:
        std::string text;
        size_t caret = 0;
        UITextSystem *textSystem = nullptr;
        uint32_t fontSize = 16;
        uint32_t textColor = 0xFF000000;
    };

} // namespace sky::ui
