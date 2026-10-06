//
// Created on 2026/10/06.
//

#pragma once

#include <editor/core/console/CommandController.h>
#include <editor/core/log/LogService.h>
#include <editor/core/property/PropertyModel.h>
#include <editor/core/selection/SelectionService.h>
#include <editor/shell/PanelView.h>
#include <editor/shell/UiDraw.h>
#include <editor/shell/UiSkin.h>
#include <editor/shell/UiTheme.h>

#include <ui/UIElement.h>
#include <ui/UIPaintContext.h>
#include <ui/UIRect.h>
#include <ui/text/UITextSystem.h>

#include <cstdint>
#include <string>

namespace sky::editor {

    namespace uc = uidraw;

    inline constexpr float kPanelInset = 10.0f;

    // Titled, themed panel frame (fallback / viewport).
    class ShellPanel : public sky::ui::UIElement, public IPanelChrome {
    public:
        explicit ShellPanel(std::string inTitle) : title(std::move(inTitle)) {}
        ~ShellPanel() override = default;

        const char *GetTypeName() const override { return "ShellPanel"; }
        void SetTextSystem(sky::ui::UITextSystem *system) { textSystem = system; }
        const std::string &GetTitle() const { return title; }
        void SetTitleBarVisible(bool visible) override
        {
            titleVisible = visible;
            MarkPaintDirty();
        }

        void OnPaint(sky::ui::UIPaintContext &context) override
        {
            UiSkin skin(GetDefaultUiTheme(), textSystem);
            skin.DrawPanel(context, GetBounds(), title, titleVisible);
        }

    private:
        std::string            title;
        sky::ui::UITextSystem *textSystem = nullptr;
        bool                   titleVisible = true;
    };

    // Base for service-backed panels: frame + title, then body lines.
    class ServicePanel : public sky::ui::UIElement, public IPanelChrome {
    public:
        ServicePanel(std::string inTitle, sky::ui::UITextSystem *text)
            : title(std::move(inTitle))
            , textSystem(text)
        {
        }
        ~ServicePanel() override = default;

        const char *GetTypeName() const override { return "ServicePanel"; }
        void SetTitleBarVisible(bool visible) override
        {
            titleVisible = visible;
            MarkPaintDirty();
        }

        void OnPaint(sky::ui::UIPaintContext &context) override
        {
            UiSkin skin(GetDefaultUiTheme(), textSystem);
            const sky::ui::UIRect content = skin.DrawPanel(context, GetBounds(), title, titleVisible);
            float y = content.top + 6.0f;
            DrawBody(context, GetBounds(), y);
        }

    protected:
        virtual void DrawBody(sky::ui::UIPaintContext &context, const sky::ui::UIRect &bounds, float &y) = 0;

        void Line(sky::ui::UIPaintContext &context, float &y, const std::string &text, uint32_t color = 0xFFDCDCDC,
                  float size = 13.0f)
        {
            if (textSystem == nullptr || y > GetBounds().bottom - 2.0f) {
                return;
            }
            const float rowH = size + 6.0f;
            uc::Text(context, text, static_cast<uint32_t>(size),
                     sky::ui::UIRect{GetBounds().left + kPanelInset, y, GetBounds().right - kPanelInset, y + rowH},
                     color, textSystem);
            y += rowH;
        }

        std::string            title;
        sky::ui::UITextSystem *textSystem = nullptr;
        bool                   titleVisible = true;
    };

    class OutlinerPanel : public ServicePanel {
    public:
        OutlinerPanel(SelectionService *inSelection, sky::ui::UITextSystem *text, std::string inTitle)
            : ServicePanel(std::move(inTitle), text)
            , selection(inSelection)
        {
        }

    protected:
        void DrawBody(sky::ui::UIPaintContext &context, const sky::ui::UIRect & /*bounds*/, float &y) override
        {
            if (selection == nullptr) {
                Line(context, y, "(selection service unavailable)");
                return;
            }
            const auto &items = selection->GetSelection();
            if (items.empty()) {
                Line(context, y, "(no selection)");
                return;
            }
            for (const auto &item : items) {
                const char *kind = item.type == SelectionType::ENTITY
                                       ? "Entity"
                                       : (item.type == SelectionType::ASSET ? "Asset" : "?");
                Line(context, y, std::string(kind) + " " + item.id.ToString());
            }
        }

    private:
        SelectionService *selection = nullptr;
    };

    class InspectorPanel : public ServicePanel {
    public:
        InspectorPanel(PropertyModel *inModel, sky::ui::UITextSystem *text, std::string inTitle)
            : ServicePanel(std::move(inTitle), text)
            , model(inModel)
        {
        }

    protected:
        void DrawBody(sky::ui::UIPaintContext &context, const sky::ui::UIRect & /*bounds*/, float &y) override
        {
            if (model == nullptr || !model->IsValid()) {
                Line(context, y, "(no object selected)");
                return;
            }
            for (const auto &descriptor : model->GetDescriptors()) {
                Line(context, y, descriptor.GetDisplayName());
            }
        }

    private:
        PropertyModel *model = nullptr;
    };

    class OutputLogPanel : public ServicePanel {
    public:
        OutputLogPanel(LogService *inLog, sky::ui::UITextSystem *text, std::string inTitle)
            : ServicePanel(std::move(inTitle), text)
            , log(inLog)
        {
        }

    protected:
        void DrawBody(sky::ui::UIPaintContext &context, const sky::ui::UIRect & /*bounds*/, float &y) override
        {
            if (log == nullptr) {
                Line(context, y, "(log service unavailable)");
                return;
            }
            const auto &entries = log->GetVisibleEntries();
            if (entries.empty()) {
                Line(context, y, "(no log entries)");
                return;
            }
            constexpr size_t kMaxLines = 24;
            const size_t     start = entries.size() > kMaxLines ? entries.size() - kMaxLines : 0;
            for (size_t i = start; i < entries.size(); ++i) {
                Line(context, y, "[" + entries[i].tag + "] " + entries[i].message);
            }
        }

    private:
        LogService *log = nullptr;
    };

    class ConsolePanel : public ServicePanel {
    public:
        ConsolePanel(CommandController *inController, sky::ui::UITextSystem *text, std::string inTitle)
            : ServicePanel(std::move(inTitle), text)
            , controller(inController)
        {
        }

    protected:
        void DrawBody(sky::ui::UIPaintContext &context, const sky::ui::UIRect & /*bounds*/, float &y) override
        {
            Line(context, y, "> _", 0xFFFFFFFF);
            if (controller != nullptr) {
                Line(context, y, "history: " + std::to_string(controller->GetHistory().Size()));
            }
        }

    private:
        CommandController *controller = nullptr;
    };

} // namespace sky::editor
