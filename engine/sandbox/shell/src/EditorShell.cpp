//
// Created on 2026/09/22.
//

#include <editor/shell/EditorShell.h>

#include <ui/UIElement.h>
#include <ui/UIContext.h>
#include <ui/UIEventRouter.h>
#include <ui/UIPaintContext.h>
#include <ui/UIStyle.h>
#include <ui/text/UITextLayout.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <functional>
#include <core/logger/Logger.h>

static const char *TAG = "EditorShell";

namespace sky::editor {

    namespace {

        constexpr uint32_t kPanelBg    = 0xFF201C18; // fallback (ABGR)
        constexpr uint32_t kTitleColor = 0xFFDEC4B0;
        constexpr uint32_t kBarBg      = 0xFF2A2F3A;
        constexpr float    kTitleSize  = 14.0f;
        constexpr float    kInset      = 8.0f;
        constexpr float    kTabHeaderH = 22.0f;
        constexpr float    kToolItemW  = 120.0f;

        uint32_t PanelBackground(const sky::ui::UITheme *theme)
        {
            if (theme != nullptr) {
                const sky::ui::UIStyle style = theme->Resolve({"panel"});
                if (style.backgroundColor != 0) {
                    return style.backgroundColor;
                }
            }
            return kPanelBg;
        }

        uint32_t PanelTitleColor(const sky::ui::UITheme *theme)
        {
            if (theme != nullptr) {
                const sky::ui::UIStyle style = theme->Resolve({"panel-title"});
                return style.textColor;
            }
            return kTitleColor;
        }

        // Titled, themed panel frame (fallback / viewport).
        class ShellPanel : public sky::ui::UIElement {
        public:
            explicit ShellPanel(std::string inTitle) : title(std::move(inTitle)) {}
            ~ShellPanel() override = default;

            const char *GetTypeName() const override { return "ShellPanel"; }
            void SetTextSystem(sky::ui::UITextSystem *system) { textSystem = system; }
            const std::string &GetTitle() const { return title; }

            void OnPaint(sky::ui::UIPaintContext &context) override
            {
                context.AddRect(GetBounds(), PanelBackground(context.GetTheme()));
                if (textSystem != nullptr && !title.empty()) {
                    sky::ui::UITextLayout::Emit(context, title, kTitleSize, GetBounds().left + kInset,
                                                GetBounds().top + 4.0f, PanelTitleColor(context.GetTheme()),
                                                textSystem->GetAtlas());
                }
            }

        private:
            std::string           title;
            sky::ui::UITextSystem *textSystem = nullptr;
        };

        // Base for service-backed panels: frame + title, then body lines.
        class ServicePanel : public sky::ui::UIElement {
        public:
            ServicePanel(std::string inTitle, sky::ui::UITextSystem *text)
                : title(std::move(inTitle))
                , textSystem(text)
            {
            }
            ~ServicePanel() override = default;

            const char *GetTypeName() const override { return "ServicePanel"; }

            void OnPaint(sky::ui::UIPaintContext &context) override
            {
                context.AddRect(GetBounds(), PanelBackground(context.GetTheme()));
                if (textSystem == nullptr) {
                    return;
                }
                sky::ui::UITextLayout::Emit(context, title, kTitleSize, GetBounds().left + kInset,
                                            GetBounds().top + 4.0f, PanelTitleColor(context.GetTheme()),
                                            textSystem->GetAtlas());
                float y = GetBounds().top + 26.0f;
                DrawBody(context, GetBounds(), y);
            }

        protected:
            virtual void DrawBody(sky::ui::UIPaintContext &context, const sky::ui::UIRect &bounds, float &y) = 0;

            void Line(sky::ui::UIPaintContext &context, float &y, const std::string &text,
                      uint32_t color = 0xFFE0E0E0, float size = 13.0f)
            {
                if (textSystem == nullptr || y > GetBounds().bottom - 2.0f) {
                    return;
                }
                sky::ui::UITextLayout::Emit(context, text, size, GetBounds().left + kInset, y, color,
                                            textSystem->GetAtlas());
                y += 18.0f;
            }

            std::string           title;
            sky::ui::UITextSystem *textSystem = nullptr;
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

        // Tab header row: one title per panel, click switches the active panel.
        class TabHeader : public sky::ui::UIElement {
        public:
            TabHeader(std::vector<std::string> inTitles, int32_t active, sky::ui::UITextSystem *text,
                      std::function<void(int32_t)> onSelect)
                : titles(std::move(inTitles))
                , activeIndex(active)
                , textSystem(text)
                , select(std::move(onSelect))
            {
            }

            const char *GetTypeName() const override { return "TabHeader"; }

            void OnPaint(sky::ui::UIPaintContext &context) override
            {
                const sky::ui::UIRect b = GetBounds();
                context.AddRect(b, kBarBg);
                if (textSystem == nullptr || titles.empty()) {
                    return;
                }
                const float span = std::max(1.0f, b.right - b.left);
                const float cell = span / static_cast<float>(titles.size());
                for (size_t i = 0; i < titles.size(); ++i) {
                    const float left = b.left + cell * static_cast<float>(i);
                    if (static_cast<int32_t>(i) == activeIndex) {
                        context.AddRect(sky::ui::UIRect{left, b.top, left + cell, b.bottom}, kPanelBg);
                    }
                    sky::ui::UITextLayout::Emit(context, titles[i], 13.0f, left + 8.0f, b.top + 3.0f, 0xFFE0E0E0,
                                                textSystem->GetAtlas());
                }
            }

            sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override
            {
                if (event.action != sky::ui::UIPointerAction::DOWN || titles.empty()) {
                    return sky::ui::UIEventResult::UNHANDLED;
                }
                const sky::ui::UIRect b = GetBounds();
                const float t = (event.x - b.left) / std::max(1.0f, b.right - b.left);
                int32_t index = static_cast<int32_t>(t * static_cast<float>(titles.size()));
                index = std::clamp(index, 0, static_cast<int32_t>(titles.size()) - 1);
                if (select) {
                    select(index);
                }
                return sky::ui::UIEventResult::HANDLED;
            }

        private:
            std::vector<std::string>     titles;
            int32_t                      activeIndex;
            sky::ui::UITextSystem       *textSystem;
            std::function<void(int32_t)> select;
        };

        // Engine-drawn tool/menu bar: labeled action items in a row.
        class ToolBar : public sky::ui::UIElement {
        public:
            struct Item {
                std::string           label;
                std::function<void()> action;
            };

            ToolBar(std::vector<Item> inItems, sky::ui::UITextSystem *text)
                : items(std::move(inItems))
                , textSystem(text)
            {
            }

            const char *GetTypeName() const override { return "ToolBar"; }

            void OnPaint(sky::ui::UIPaintContext &context) override
            {
                const sky::ui::UIRect b = GetBounds();
                context.AddRect(b, 0xFF1C2026);
                if (textSystem == nullptr) {
                    return;
                }
                for (size_t i = 0; i < items.size(); ++i) {
                    const float left = b.left + kToolItemW * static_cast<float>(i);
                    if (static_cast<int32_t>(i) == hovered) {
                        context.AddRect(sky::ui::UIRect{left, b.top, left + kToolItemW, b.bottom}, kBarBg);
                    }
                    sky::ui::UITextLayout::Emit(context, items[i].label, 13.0f, left + 8.0f, b.top + 4.0f,
                                                0xFFE0E0E0, textSystem->GetAtlas());
                }
            }

            sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &event) override
            {
                const sky::ui::UIRect b = GetBounds();
                if (event.action == sky::ui::UIPointerAction::MOVE) {
                    hovered = (event.x >= b.left && event.x < b.left + kToolItemW * static_cast<float>(items.size()))
                                  ? static_cast<int32_t>((event.x - b.left) / kToolItemW)
                                  : -1;
                    return sky::ui::UIEventResult::UNHANDLED;
                }
                if (event.action != sky::ui::UIPointerAction::DOWN) {
                    return sky::ui::UIEventResult::UNHANDLED;
                }
                if (event.x < b.left || event.x >= b.left + kToolItemW * static_cast<float>(items.size())) {
                    return sky::ui::UIEventResult::UNHANDLED;
                }
                const size_t index = static_cast<size_t>((event.x - b.left) / kToolItemW);
                if (index < items.size() && items[index].action) {
                    items[index].action();
                }
                return sky::ui::UIEventResult::HANDLED;
            }

        private:
            std::vector<Item>      items;
            sky::ui::UITextSystem *textSystem;
            int32_t                hovered = -1;
        };

        std::vector<sky::ui::UIRect> SplitRect(const sky::ui::UIRect &rect, SplitOrientation orientation,
                                               const std::vector<float> &ratios, uint32_t count)
        {
            std::vector<sky::ui::UIRect> out;
            out.reserve(count);
            if (count == 0) {
                return out;
            }
            std::vector<float> weights(count, 1.0f);
            if (ratios.size() == count) {
                weights = ratios;
            }
            float total = 0.0f;
            for (float w : weights) {
                total += std::max(w, 0.0f);
            }
            if (total <= 0.0f) {
                std::fill(weights.begin(), weights.end(), 1.0f);
                total = static_cast<float>(count);
            }
            const float span = orientation == SplitOrientation::HORIZONTAL ? (rect.right - rect.left)
                                                                           : (rect.bottom - rect.top);
            float cursor = orientation == SplitOrientation::HORIZONTAL ? rect.left : rect.top;
            for (uint32_t i = 0; i < count; ++i) {
                const float extent = span * (weights[i] / total);
                sky::ui::UIRect slot = rect;
                if (orientation == SplitOrientation::HORIZONTAL) {
                    slot.left  = cursor;
                    slot.right = (i + 1 == count) ? rect.right : cursor + extent;
                    cursor     = slot.right;
                } else {
                    slot.top    = cursor;
                    slot.bottom = (i + 1 == count) ? rect.bottom : cursor + extent;
                    cursor      = slot.bottom;
                }
                out.push_back(slot);
            }
            return out;
        }

    } // namespace

    EditorShell::EditorShell()
        : context(std::make_unique<sky::ui::UIContext>())
    {
        eventRouter = std::make_unique<sky::ui::UIEventRouter>(*context);
        ApplyTheme();
    }

    EditorShell::~EditorShell() = default;

    void EditorShell::ApplyTheme()
    {
        sky::ui::UITheme &theme = context->GetTheme();

        sky::ui::UIStyle panel;
        panel.SetBackgroundColor(0xFF201C18);
        panel.SetBorderColor(0xFF3A2F2A);
        panel.SetBorderWidth(1.0f);
        theme.SetStyle("panel", panel);

        sky::ui::UIStyle title;
        title.SetTextColor(0xFFDEC4B0);
        theme.SetStyle("panel-title", title);

        sky::ui::UIStyle bar;
        bar.SetBackgroundColor(0xFF2A2F3A);
        bar.SetTextColor(0xFFE0E0E0);
        theme.SetStyle("menu-bar", bar);

        sky::ui::UIStyle item;
        item.SetButtonColors(0x00000000, 0xFF3A4150, 0xFF2A2F3A);
        item.SetTextColor(0xFFE0E0E0);
        theme.SetStyle("menu-item", item);
    }

    void EditorShell::SetTextSystem(sky::ui::UITextSystem *text) { textSystem = text; }
    void EditorShell::SetLayout(LayoutModel *layout) { layoutModel = layout; }
    void EditorShell::SetPanelRegistry(PanelRegistry *registry) { panelRegistry = registry; }
    void EditorShell::SetSelection(SelectionService *value) { selection = value; }
    void EditorShell::SetLogService(LogService *value) { logService = value; }
    void EditorShell::SetCommandController(CommandController *value) { commandController = value; }
    void EditorShell::SetInspectorModel(PropertyModel *model) { inspectorModel = model; }

    void EditorShell::RegisterPanelView(const std::string &panelId, PanelViewFactory factory)
    {
        viewFactories[panelId] = std::move(factory);
    }

    void EditorShell::RegisterBuiltinPanelViews()
    {
        auto titleOf = [this](const char *id, const char *fallback) -> std::string {
            if (panelRegistry != nullptr) {
                if (const PanelInfo *info = panelRegistry->Find(id)) {
                    return info->title;
                }
            }
            return fallback;
        };

        RegisterPanelView("viewport", [this, t = titleOf("viewport", "Viewport")]() {
            return std::unique_ptr<sky::ui::UIElement>(new ShellPanel(t));
        });
        RegisterPanelView("outliner", [this, t = titleOf("outliner", "Outliner")]() {
            return std::unique_ptr<sky::ui::UIElement>(new OutlinerPanel(selection, textSystem, t));
        });
        RegisterPanelView("inspector", [this, t = titleOf("inspector", "Inspector")]() {
            return std::unique_ptr<sky::ui::UIElement>(new InspectorPanel(inspectorModel, textSystem, t));
        });
        RegisterPanelView("outputlog", [this, t = titleOf("outputlog", "Output Log")]() {
            return std::unique_ptr<sky::ui::UIElement>(new OutputLogPanel(logService, textSystem, t));
        });
        RegisterPanelView("console", [this, t = titleOf("console", "Console")]() {
            return std::unique_ptr<sky::ui::UIElement>(new ConsolePanel(commandController, textSystem, t));
        });
    }

    sky::ui::UIElement *EditorShell::CreatePanelView(const std::string &panelId)
    {
        std::string title = panelId;
        if (panelRegistry != nullptr) {
            if (const PanelInfo *info = panelRegistry->Find(panelId)) {
                title = info->title;
            }
        }

        std::unique_ptr<sky::ui::UIElement> element;
        const auto it = viewFactories.find(panelId);
        if (it != viewFactories.end() && it->second) {
            element = it->second();
        }
        if (element == nullptr) {
            element = std::make_unique<ShellPanel>(title);
        }

        element->AddStyleClass("panel");
        if (auto *frame = dynamic_cast<ShellPanel *>(element.get())) {
            frame->SetTextSystem(textSystem);
        }
        element->SetName(panelId);
        return context->AddChild(std::move(element));
    }

    void EditorShell::CreateNode(LayoutNode *node, const sky::ui::UIRect &rect)
    {
        if (node == nullptr) {
            return;
        }
        if (IsTab(node)) {
            auto *tab = static_cast<TabNode *>(node);
            if (tab->panels.empty()) {
                return;
            }
            std::vector<std::string> ids;
            std::vector<std::string> titles;
            for (const auto &tabPanel : tab->panels) {
                if (panelRegistry != nullptr && !panelRegistry->Contains(tabPanel.panelId)) {
                    continue;
                }
                ids.push_back(tabPanel.panelId);
                titles.push_back(panelRegistry != nullptr && panelRegistry->Find(tabPanel.panelId) != nullptr
                                     ? panelRegistry->Find(tabPanel.panelId)->title
                                     : tabPanel.panelId);
            }
            if (ids.empty()) {
                return;
            }
            int32_t index = tab->activeIndex;
            if (index < 0 || index >= static_cast<int32_t>(ids.size())) {
                index = 0;
            }

            Slot slot;
            if (ids.size() > 1) {
                auto header = std::make_unique<TabHeader>(titles, index, textSystem,
                                                          [this, tab](int32_t idx) {
                                                              tab->activeIndex = idx;
                                                              pendingRebuild = true;
                                                          });
                slot.header = context->AddChild(std::move(header));
            }
            if (sky::ui::UIElement *body = CreatePanelView(ids[static_cast<size_t>(index)])) {
                slot.body = body;
                panels.push_back(body);
            }
            slots.push_back(slot);
            return;
        }
        if (IsSplit(node)) {
            auto *split = static_cast<SplitNode *>(node);
            if (split->children.empty()) {
                return;
            }
            const auto slotsRect = SplitRect(rect, split->orientation, split->ratios,
                                             static_cast<uint32_t>(split->children.size()));
            for (size_t i = 0; i < split->children.size(); ++i) {
                CreateNode(split->children[i].get(), slotsRect[i]);
            }
        }
    }

    void EditorShell::ApplyNode(LayoutNode *node, const sky::ui::UIRect &rect, size_t &panelCursor)
    {
        if (node == nullptr) {
            return;
        }
        if (IsTab(node)) {
            auto *tab = static_cast<TabNode *>(node);
            if (tab->panels.empty()) {
                return;
            }
            std::vector<std::string> ids;
            for (const auto &tabPanel : tab->panels) {
                if (panelRegistry != nullptr && !panelRegistry->Contains(tabPanel.panelId)) {
                    continue;
                }
                ids.push_back(tabPanel.panelId);
            }
            if (ids.empty()) {
                return;
            }
            if (panelCursor >= slots.size()) {
                return;
            }
            Slot &slot = slots[panelCursor++];
            sky::ui::UIRect bodyRect = rect;
            if (slot.header != nullptr) {
                sky::ui::UIRect headerRect = rect;
                headerRect.bottom = rect.top + kTabHeaderH;
                slot.header->SetBounds(headerRect);
                bodyRect.top = headerRect.bottom;
            }
            if (slot.body != nullptr) {
                slot.body->SetBounds(bodyRect);
            }
            return;
        }
        if (IsSplit(node)) {
            auto *split = static_cast<SplitNode *>(node);
            if (split->children.empty()) {
                return;
            }
            const auto slotsRect = SplitRect(rect, split->orientation, split->ratios,
                                             static_cast<uint32_t>(split->children.size()));
            for (size_t i = 0; i < split->children.size(); ++i) {
                ApplyNode(split->children[i].get(), slotsRect[i], panelCursor);
            }
        }
    }

    void EditorShell::Rebuild()
    {
        // Drop focus/hover/capture first: rebuilding destroys the elements and
        // would leave the router holding dangling pointers.
        eventRouter->Reset();
        context->GetRoot()->ClearChildren();
        panels.clear();
        slots.clear();
        built = false;
        pendingRebuild = false;

        // Tool/menu bar: one "Show/Hide <panel>" item per built-in panel.
        std::vector<ToolBar::Item> items;
        static const char *kIds[] = {"viewport", "outliner", "inspector", "outputlog", "console"};
        std::vector<std::string> present;
        if (layoutModel != nullptr) {
            layoutModel->CollectPanels(present);
        }
        for (const char *id : kIds) {
            std::string panelId = id;
            if (panelRegistry != nullptr && !panelRegistry->Contains(panelId)) {
                continue;
            }
            std::string title = panelRegistry != nullptr && panelRegistry->Find(panelId) != nullptr
                                    ? panelRegistry->Find(panelId)->title
                                    : panelId;
            const bool shown = std::find(present.begin(), present.end(), panelId) != present.end();
            items.push_back({(shown ? "Hide " : "Show ") + title,
                             [this, panelId, shown]() { SetPanelVisible(panelId, !shown); }});
        }
        items.push_back({"About", []() { LOG_I(TAG, "SkyEngine Editor (sandbox shell)"); }});

        auto toolbar = std::make_unique<ToolBar>(std::move(items), textSystem);
        toolbarElement = context->AddChild(std::move(toolbar));

        if (layoutModel != nullptr && !layoutModel->IsEmpty()) {
            const sky::ui::UIRect rect{0.0f, toolbarHeight, width, height};
            CreateNode(layoutModel->GetRoot(), rect);
        }

        built = true;
        LOG_I(TAG, "editor shell built (%zu panels)", panels.size());
    }

    void EditorShell::SetPanelVisible(const std::string &panelId, bool visible)
    {
        if (layoutModel == nullptr) {
            return;
        }
        std::vector<std::string> present;
        layoutModel->CollectPanels(present);
        const bool isPresent = std::find(present.begin(), present.end(), panelId) != present.end();
        if (visible == isPresent) {
            return;
        }
        if (!visible) {
            layoutModel->ClosePanel(panelId);
        } else {
            std::string anchor = "viewport";
            if (std::find(present.begin(), present.end(), anchor) == present.end()) {
                anchor = present.empty() ? std::string() : present.front();
            }
            if (!anchor.empty()) {
                layoutModel->SplitPanel(anchor, SplitOrientation::VERTICAL, panelId);
            }
        }
        pendingRebuild = true;
    }

    void EditorShell::Layout(float inWidth, float inHeight)
    {
        width  = inWidth > 0.0f ? inWidth : 1.0f;
        height = inHeight > 0.0f ? inHeight : 1.0f;
        context->SetContentSize(width, height);

        if (pendingRebuild) {
            Rebuild();
        }

        if (toolbarElement != nullptr) {
            toolbarElement->SetBounds(sky::ui::UIRect{0.0f, 0.0f, width, toolbarHeight});
        }

        if (layoutModel != nullptr && !layoutModel->IsEmpty()) {
            const sky::ui::UIRect rect{0.0f, toolbarHeight, width, height};
            size_t             cursor = 0;
            ApplyNode(layoutModel->GetRoot(), rect, cursor);
        }
    }

    void EditorShell::Paint(sky::ui::UIPaintContext &paintContext)
    {
        context->Paint(paintContext);
    }

    sky::ui::UIElement *EditorShell::HitTest(float x, float y) const
    {
        return eventRouter != nullptr ? eventRouter->HitTest(x, y) : nullptr;
    }

    bool EditorShell::DispatchPointer(const sky::ui::UIPointerEvent &event)
    {
        if (eventRouter == nullptr || context == nullptr) {
            return false;
        }
        const bool overUI = eventRouter->HitTest(event.x, event.y) != nullptr;
        context->SetWantsInput(overUI || eventRouter->GetFocus() != nullptr);
        return eventRouter->DispatchPointer(event) == sky::ui::UIEventResult::HANDLED;
    }

    bool EditorShell::DispatchKey(const sky::ui::UIKeyEvent &event)
    {
        if (eventRouter == nullptr || context == nullptr) {
            return false;
        }
        context->SetWantsInput(eventRouter->GetFocus() != nullptr);
        return eventRouter->DispatchKey(event) == sky::ui::UIEventResult::HANDLED;
    }

    bool EditorShell::DispatchText(const sky::ui::UITextInputEvent &event)
    {
        if (eventRouter == nullptr) {
            return false;
        }
        return eventRouter->DispatchText(event) == sky::ui::UIEventResult::HANDLED;
    }

    bool EditorShell::WantsInput() const
    {
        return context != nullptr && context->WantsInput();
    }

    bool EditorShell::HasPanel(const std::string &panelId) const
    {
        return std::any_of(panels.begin(), panels.end(), [&panelId](const sky::ui::UIElement *element) {
            return element != nullptr && element->GetName() == panelId;
        });
    }

} // namespace sky::editor
