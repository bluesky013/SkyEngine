//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIElement.h>

#include <cstdint>
#include <string>

namespace sky::ui {

    class UITextSystem;

    class Text : public UIElement {
    public:
        enum class Align {
            LEFT = 0,
            CENTER,
            RIGHT,
        };

        Text() = default;
        ~Text() override = default;

        const char *GetTypeName() const override { return "Text"; }

        void SetTextSystem(UITextSystem *value);
        UITextSystem *GetTextSystem() const { return textSystem; }

        void SetContent(const std::string &value);
        const std::string &GetContent() const { return content; }

        // Localization key; when set it is resolved at measure/paint time.
        void SetTextKey(const std::string &value);
        const std::string &GetTextKey() const { return textKey; }
        std::string GetResolvedText() const;

        void SetFontSize(uint32_t value);
        uint32_t GetFontSize() const { return fontSize; }

        void SetTextColor(uint32_t value);
        uint32_t GetTextColor() const { return textColor; }

        void SetAlign(Align value);
        Align GetAlign() const { return align; }

        // AUTO sizing comes from the measured text bounds.
        void Measure(float &outWidth, float &outHeight) override;
        void OnPaint(UIPaintContext &context) override;
        bool SetProperty(const std::string &path, const UIPropertyValue &value) override;

    private:
        UITextSystem *textSystem = nullptr;
        std::string content;
        std::string textKey;
        uint32_t fontSize = 16;
        uint32_t textColor = 0xFF000000;
        Align align = Align::LEFT;
    };

} // namespace sky::ui
