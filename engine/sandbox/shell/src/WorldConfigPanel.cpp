//
// Created on 2026/10/07.
//

#include <editor/shell/WorldConfigPanel.h>

#include <editor/shell/UiDraw.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <framework/serialization/SerializationContext.h>
#include <framework/world/WorldSubSystemRegistry.h>

#include <string>
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

    WorldConfigPanel::WorldConfigPanel(sky::ui::UITextSystem *text, DocumentProvider provider, std::string inTitle)
        : textSystem(text), skin(GetDefaultUiTheme(), text), documentProvider(std::move(provider)), title(std::move(inTitle))
    {
        // Create the reflected-form view once, here (never during OnPaint) so the
        // element tree is not mutated while painting.
        formView = static_cast<ReflectedFormView *>(AddChild(std::make_unique<ReflectedFormView>(text, "")));
        // Edits to a subsystem's config mark the world document dirty so the
        // title/status show the unsaved marker and Save persists them.
        formView->SetOnEdited([this]() {
            if (WorldDocument *doc = documentProvider ? documentProvider() : nullptr) {
                doc->MarkDirty();
            }
        });
        Rebind();
    }

    void WorldConfigPanel::SetTitleBarVisible(bool visible)
    {
        titleVisible = visible;
        MarkPaintDirty();
    }

    sky::ui::UIRect WorldConfigPanel::ListRect() const
    {
        const sky::ui::UIRect &b = GetBounds();
        const UiMetrics       &m = skin.Theme().metrics;
        return sky::ui::UIRect{b.left, b.top + m.headerHeight, b.left + m.listColumnWidth, b.bottom};
    }

    sky::ui::UIRect WorldConfigPanel::RowRect(int index) const
    {
        const UiMetrics      &m     = skin.Theme().metrics;
        const sky::ui::UIRect list  = ListRect();
        const float           inset = m.controlPad * 0.5f;
        const float           top   = list.top + static_cast<float>(index) * m.rowHeight;
        return sky::ui::UIRect{list.left + inset, top, list.right - inset, top + m.rowHeight - m.checkboxPad};
    }

    sky::ui::UIRect WorldConfigPanel::CheckRect(int index) const
    {
        const UiMetrics      &m   = skin.Theme().metrics;
        const sky::ui::UIRect row = RowRect(index);
        const float           cy  = (row.top + row.bottom) * 0.5f;
        const float           pad = m.checkboxPad;
        const float           sz  = m.checkboxSize;
        return sky::ui::UIRect{row.left + pad, cy - sz * 0.5f, row.left + pad + sz, cy + sz * 0.5f};
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
        // Persist edits made through the form before switching subsystem.
        if (WorldDocument *doc = documentProvider ? documentProvider() : nullptr; doc != nullptr && doc->IsDirty()) {
            doc->Save();
        }
        selected = index;
        Rebind();
        MarkPaintDirty();
    }

    void WorldConfigPanel::ToggleEnabled(int index)
    {
        WorldDocument *doc = documentProvider ? documentProvider() : nullptr;
        if (doc == nullptr) {
            return;
        }
        const std::vector<Name> names = RegistryNames();
        if (index < 0 || index >= static_cast<int>(names.size())) {
            return;
        }
        const std::string name(names[static_cast<std::size_t>(index)].GetStr());

        bool       flag   = false;
        const bool inDesc = doc->IsSubSystemEnabled(name, flag);
        doc->SetSubSystemEnabled(name, !(!inDesc || flag)); // registered-and-absent counts as enabled
        doc->Rebuild();                                     // apply immediately (adds newly-enabled)
        doc->Save();                                        // persist the selection

        // SetSubSystemEnabled can append to WorldDesc::subSystems, which
        // reallocates the vector; Any stores its value inline, so the form's
        // bound config pointer is now dangling. Rebind to the fresh instance.
        Rebind();
        MarkPaintDirty();
    }

    void WorldConfigPanel::Rebind()
    {
        if (formView == nullptr) {
            return;
        }
        // No world open: the panel shows an empty state, never fabricated defaults.
        WorldDocument *doc = documentProvider ? documentProvider() : nullptr;
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
    }

    void WorldConfigPanel::OnPaint(sky::ui::UIPaintContext &context)
    {
        const sky::ui::UIRect content = skin.DrawPanel(context, GetBounds(), title, titleVisible);
        const UiTheme        &th      = skin.Theme();

        // Open/close a world while the panel exists: rebind to the new document.
        WorldDocument *doc = documentProvider ? documentProvider() : nullptr;
        if (doc != lastDocument) {
            lastDocument = doc;
            selected     = 0;
            Rebind();
        }
        if (formView == nullptr) {
            return;
        }

        // Empty state before any world is opened: no fabricated config values.
        if (doc == nullptr) {
            formView->SetVisible(false);
            uc::Text(context, "No world open", th.fonts.label, content, th.colors.textMuted, textSystem, uc::HAlign::Center, uc::VAlign::Middle,
                     false);
            return;
        }

        const sky::ui::UIRect   list  = ListRect();
        const std::vector<Name> names = RegistryNames();

        uc::Fill(context, list, th.colors.section);
        uc::HLine(context, list.right, list.right, list.bottom, th.colors.border);

        for (int i = 0; i < static_cast<int>(names.size()); ++i) {
            const sky::ui::UIRect row = RowRect(i);
            if (i == selected) {
                uc::Fill(context, row, th.colors.rowSelected);
            }
            bool       flag   = false;
            const bool inDesc = doc->IsSubSystemEnabled(std::string(names[static_cast<std::size_t>(i)].GetStr()), flag);
            skin.DrawCheckbox(context, CheckRect(i), !inDesc || flag, false);
            uc::Text(context, std::string(names[static_cast<std::size_t>(i)].GetStr()), th.fonts.label,
                     sky::ui::UIRect{row.left + th.metrics.listLabelIndent, row.top, row.right, row.bottom}, th.colors.text, textSystem,
                     uc::HAlign::Left, uc::VAlign::Middle, true);
        }

        // The reflected-form child occupies the right pane.
        formView->SetBounds(sky::ui::UIRect{content.left + th.metrics.listColumnWidth, content.top, content.right, content.bottom});
        formView->SetVisible(!names.empty());
    }

    sky::ui::UIEventResult WorldConfigPanel::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (event.action != sky::ui::UIPointerAction::DOWN) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        WorldDocument *doc = documentProvider ? documentProvider() : nullptr;
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
