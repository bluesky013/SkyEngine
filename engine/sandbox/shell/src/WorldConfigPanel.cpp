//
// Created on 2026/10/07.
//

#include <editor/core/EditorCore.h>
#include <editor/shell/WorldConfigPanel.h>

#include <editor/ui/UiDraw.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <framework/serialization/SerializationContext.h>
#include <framework/world/WorldSubSystemRegistry.h>

#include <algorithm>
#include <string>
#include <ui/widgets/DockPanel.h>
#include <utility>
#include <vector>

namespace sky::editor {

    namespace uc = uidraw;

    namespace {
        std::vector<Name> RegistryNames()
        {
            return WorldSubSystemRegistry::Get().GetNames();
        }
    } // namespace

    struct WorldConfigPanel::Layout {
        sky::ui::DockPanel *panel  = nullptr;
        sky::ui::UIElement *header = nullptr;
        sky::ui::DockPanel *body   = nullptr;
        sky::ui::UIElement *list   = nullptr;
    };

    WorldConfigPanel::WorldConfigPanel(sky::ui::UITextSystem *text, DocumentProvider provider, std::string inTitle)
        : layout(std::make_unique<Layout>())
        , textSystem(text)
        , skin(GetDefaultUiTheme(), text)
        , documentProvider(std::move(provider))
        , title(std::move(inTitle))
    {
        // Create the reflected-form view once, here (never during OnPaint) so the
        // element tree is not mutated while painting.
        using namespace sky::ui;
        layout->panel = static_cast<DockPanel *>(AddChild(std::make_unique<DockPanel>()));
        auto header   = std::make_unique<UIElement>();
        header->SetLayoutVolatile(false);
        layout->header = layout->panel->Add(std::move(header), {DockSide::Top});
        layout->body   = static_cast<DockPanel *>(layout->panel->Add(std::make_unique<DockPanel>()));
        auto list      = std::make_unique<UIElement>();
        list->SetLayoutVolatile(false);
        layout->list = layout->body->Add(std::move(list), {DockSide::Left});
        formView     = static_cast<ReflectedFormView *>(layout->body->Add(std::make_unique<ReflectedFormView>(text, "")));
        // Edits to a subsystem's config mark the world document dirty so the
        // title/status show the unsaved marker and Save persists them.
        formView->SetOnEdited([this]() {
            if (lastDocument != nullptr) {
                lastDocument->MarkDirty();
            }
        });
        SynchronizeDocument();
    }

    WorldConfigPanel::~WorldConfigPanel() = default;

    void WorldConfigPanel::UpdateComposition(bool arranging)
    {
        const auto  size   = GetArrangedSize();
        const auto &m      = skin.Theme().metrics;
        const float header = titleVisible ? std::min(std::max(0.0f, m.panelHeaderHeight), size.height) : 0.0f;
        layout->panel->SetSlot(layout->header, {sky::ui::DockSide::Top, header});
        layout->body->SetSlot(layout->list, {sky::ui::DockSide::Left, std::min(std::max(0.0f, m.listColumnWidth), size.width * 0.5f)});
        if (arranging) {
            // Slot changes above are inputs consumed by this allocation. Clear
            // them before arranging children, so a child that invalidates the
            // panel during its own arrangement still schedules another pass.
            ClearLayoutDirty();
        }
        layout->panel->ArrangeIfNeeded(GetBounds(), size);
    }

    void WorldConfigPanel::SetTitleBarVisible(bool visible)
    {
        if (titleVisible == visible) {
            return;
        }
        titleVisible = visible;
        MarkLayoutDirty();
        MarkPaintDirty();
    }

    void WorldConfigPanel::SetDocument(WorldDocument *document)
    {
        const bool changed = providerMode || lastDocument != document;
        providerMode       = false;
        SetLayoutVolatile(false);
        if (formView != nullptr) {
            formView->SetLayoutVolatile(false);
        }
        if (!changed) {
            return;
        }
        lastDocument = document;
        selected     = 0;
        RefreshDocument();
    }

    void WorldConfigPanel::RefreshDocument()
    {
        // Reload may already have replaced storage inside the same document.
        // Drop old callbacks before resolving or default-creating fresh config.
        if (formView != nullptr) {
            formView->Unbind();
        }
        Rebind();
        configRevision = lastDocument ? lastDocument->GetConfigRevision() : 0;
        MarkLayoutDirty();
    }

    void WorldConfigPanel::SynchronizeLayout()
    {
        SynchronizeLayout(false);
    }

    void WorldConfigPanel::SynchronizeLayout(bool arranging)
    {
        if (layoutBusy)
            return;
        layoutBusy = true;
        SynchronizeDefaultUiTheme(*this, themeRevision);
        SynchronizeDocument();
        if (formView != nullptr) {
            formView->SynchronizeLayout();
        }
        UpdateComposition(arranging);
        layoutBusy = false;
    }

    void WorldConfigPanel::SynchronizeDocument()
    {
        WorldDocument *document = providerMode ? (documentProvider ? documentProvider() : nullptr) : lastDocument;
        if (lastDocument != document) {
            lastDocument = document;
            selected     = 0;
            RefreshDocument();
        } else if (document == nullptr && formView != nullptr) {
            formView->SetVisible(false);
        }
        const auto revision = document ? document->GetConfigRevision() : 0;
        if (revision != configRevision) {
            configRevision = revision;
            RefreshDocument();
        }
    }

    sky::ui::UIRect WorldConfigPanel::ListRect() const
    {
        const_cast<WorldConfigPanel *>(this)->SynchronizeLayout();
        return layout->list->GetBounds();
    }

    sky::ui::UIRect WorldConfigPanel::RowRect(int index) const
    {
        const_cast<WorldConfigPanel *>(this)->SynchronizeLayout();
        return ComputeRowRect(index);
    }

    sky::ui::UIRect WorldConfigPanel::ComputeRowRect(int index) const
    {
        if (index < 0)
            return {};
        const auto &m     = skin.Theme().metrics;
        const auto  list  = layout->list->GetBounds();
        const float inset = std::min(std::max(0.0f, m.controlPad) * 0.5f, list.Width() * 0.5f);
        const float top   = list.top + static_cast<float>(index) * std::max(0.0f, m.rowHeight);
        return {list.left + inset, top, list.right - inset, top + std::max(0.0f, m.rowHeight - m.checkboxPad)};
    }

    sky::ui::UIRect WorldConfigPanel::CheckRect(int index) const
    {
        const_cast<WorldConfigPanel *>(this)->SynchronizeLayout();
        return ComputeCheckRect(index);
    }

    sky::ui::UIRect WorldConfigPanel::ComputeCheckRect(int index) const
    {
        const auto &m    = skin.Theme().metrics;
        const auto  row  = ComputeRowRect(index);
        const float pad  = std::min(std::max(0.0f, m.checkboxPad), row.Width() * 0.5f);
        const float size = std::min({std::max(0.0f, m.checkboxSize), std::max(0.0f, row.Width() - 2.0f * pad), row.Height()});
        const float cy   = (row.top + row.bottom) * 0.5f;
        return {row.left + pad, cy - size * 0.5f, row.left + pad + size, cy + size * 0.5f};
    }

    int WorldConfigPanel::RowAt(float x, float y) const
    {
        const sky::ui::UIRect list = ListRect();
        if (!list.Contains(x, y)) {
            return -1;
        }
        const std::vector<Name> names = RegistryNames();
        for (int i = 0; i < static_cast<int>(names.size()); ++i) {
            if (RowRect(i).Contains(x, y)) {
                return i;
            }
        }
        return -1;
    }

    void WorldConfigPanel::Select(int index)
    {
        if (index == selected) {
            return;
        }
        // This selection path still owns a live config. Restore any preview
        // before saving it and abandoning the old binding.
        if (formView != nullptr) {
            formView->CancelInteractions();
        }
        selected = index;
        Rebind();
        MarkPaintDirty();
    }

    void WorldConfigPanel::ToggleEnabled(int index)
    {
        WorldDocument *doc = lastDocument;
        if (doc == nullptr) {
            return;
        }
        const std::vector<Name> names = RegistryNames();
        if (index < 0 || index >= static_cast<int>(names.size())) {
            return;
        }
        const std::string name(names[static_cast<std::size_t>(index)].GetStr());

        // The current config is live here. Restore its preview and detach
        // before appending subsystem storage can invalidate its address.
        if (formView != nullptr) {
            formView->CancelInteractions();
            formView->Unbind();
        }

        bool flag = false;
        doc->IsSubSystemEnabled(name, flag);
        EditorCore::GetCommandService().Execute(doc->MakeSubSystemEnabledCommand(name, !flag));
        Rebind();
        MarkPaintDirty();
    }

    void WorldConfigPanel::Rebind()
    {
        if (formView == nullptr) {
            return;
        }
        // No world open: the panel shows an empty state, never fabricated defaults.
        WorldDocument *doc = lastDocument;
        formView->SetVisible(doc != nullptr && !RegistryNames().empty());
        if (doc == nullptr) {
            formView->Bind(PropertyObject{});
            return;
        }

        const std::vector<Name> names = RegistryNames();
        if (names.empty()) {
            formView->Bind(PropertyObject{});
            return;
        }
        if (selected < 0 || selected >= static_cast<int>(names.size())) {
            selected = 0;
        }

        const Name                             name = names[static_cast<std::size_t>(selected)];
        const sky::WorldSubSystemRegistration *reg  = WorldSubSystemRegistry::Get().GetRegistration(name);
        if (reg == nullptr || reg->configType == nullptr) {
            formView->Bind(PropertyObject{});
            return;
        }

        Any            *config = doc->EnsureSubSystemConfig(std::string(name.GetStr()));
        const TypeNode *node   = (config != nullptr) ? GetTypeNode(*config) : nullptr;
        if (node == nullptr) {
            formView->Bind(PropertyObject{});
            return;
        }
        formView->Bind(PropertyObject{config->Data(), node});
        formView->Form().SetEditCommandFactory([doc, key = std::string(name.GetStr())](const PropertyDescriptor &property, Any value) {
            return doc->MakeConfigEditCommand(key, property, std::move(value));
        });
    }

    void WorldConfigPanel::ArrangeChildren(const sky::ui::UIRect &)
    {
        SynchronizeLayout(true);
    }

    void WorldConfigPanel::OnPaint(sky::ui::UIPaintContext &context)
    {
        context.PushClip(GetBounds());
        skin.DrawPanel(context, GetBounds(), title + " (applies on next Play)", titleVisible);
        const sky::ui::UIRect content = layout->body->GetBounds();
        const UiTheme        &th      = skin.Theme();
        WorldDocument        *doc     = lastDocument;

        // Empty state before any world is opened: no fabricated config values.
        if (doc == nullptr) {
            uc::Text(context, "No world open", th.fonts.label, content, th.colors.textMuted, textSystem, uc::HAlign::Center, uc::VAlign::Middle,
                     false);
            context.PopClip();
            return;
        }

        const sky::ui::UIRect list = layout->list->GetBounds();
        context.PushClip(list);
        const std::vector<Name> names = RegistryNames();

        uc::Fill(context, list, th.colors.section);
        uc::HLine(context, list.right, list.right, list.bottom, th.colors.border);

        for (int i = 0; i < static_cast<int>(names.size()); ++i) {
            const sky::ui::UIRect row = ComputeRowRect(i);
            if (i == selected) {
                uc::Fill(context, row, th.colors.rowSelected);
            }
            bool       flag   = false;
            const bool inDesc = doc->IsSubSystemEnabled(std::string(names[static_cast<std::size_t>(i)].GetStr()), flag);
            skin.DrawCheckbox(context, ComputeCheckRect(i), inDesc && flag, false);
            uc::Text(context, std::string(names[static_cast<std::size_t>(i)].GetStr()), th.fonts.label,
                     sky::ui::UIRect{row.left + th.metrics.listLabelIndent, row.top, row.right, row.bottom}, th.colors.text, textSystem,
                     uc::HAlign::Left, uc::VAlign::Middle, true);
        }
        context.PopClip();
        context.PopClip();
    }

    sky::ui::UIEventResult WorldConfigPanel::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (event.action != sky::ui::UIPointerAction::DOWN) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        SynchronizeLayout();
        WorldDocument *doc = lastDocument;
        if (doc == nullptr) {
            return sky::ui::UIEventResult::UNHANDLED; // empty state: no interactive rows
        }
        const int row = RowAt(event.x, event.y);
        if (row < 0) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        if (CheckRect(row).Contains(event.x, event.y)) {
            ToggleEnabled(row);
        } else {
            Select(row);
        }
        return sky::ui::UIEventResult::HANDLED;
    }

} // namespace sky::editor
