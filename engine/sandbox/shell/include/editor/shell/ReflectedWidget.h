//
// Created on 2026/10/04.
//

#pragma once

#include <editor/core/property/PropertyEditor.h>
#include <editor/core/property/ReflectedForm.h>
#include <editor/shell/UiSkin.h>
#include <ui/UIEvent.h>
#include <ui/UIRect.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace sky::ui {
    class UIPaintContext;
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    class CommandService;
    class ReflectedWidget;

    // Services and helpers a widget needs from its host (the form view).
    struct ReflectedWidgetHost {
        virtual ~ReflectedWidgetHost() = default;
        virtual ReflectedForm &Form() = 0;
        virtual CommandService &Commands() = 0;
        virtual const UiSkin &Skin() const = 0;
        virtual sky::ui::UITextSystem *Text() const = 0;
        virtual sky::ui::UIRect ViewBounds() const = 0;
        virtual void MarkDirty() = 0;
        virtual void RefreshForm() = 0;
        virtual bool IsHovered(const PropertyField &field) const = 0;
        // Host-managed inline text edit (caret, input filter, Enter/Esc); the
        // commit callback parses and applies the text, returning success.
        virtual void BeginTextEdit(PropertyField &field, int component, PropertyEditorKind inputKind,
                                   const std::string &initial, std::function<bool(const std::string &)> commit) = 0;
    };

    // Base for per-kind property widgets. A specialized widget (e.g. Color)
    // overrides the generic behavior for its PropertyEditorKind.
    class ReflectedWidget {
    public:
        virtual ~ReflectedWidget() = default;

        virtual PropertyEditorKind Kind() const = 0;

        // Number of form rows this widget occupies (multi-row widgets draw their
        // own sub-rows inside the given tall rect).
        virtual int RowCount() const { return 1; }

        // Draws the control inside rect for the given field.
        virtual void Paint(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context, PropertyField &field,
                           const sky::ui::UIRect &rect) = 0;

        // Pointer DOWN inside the field's control; return true to become the active
        // widget and receive OnMove/OnUp until release.
        virtual bool OnDown(ReflectedWidgetHost &host, PropertyField &field, const sky::ui::UIPointerEvent &event,
                            const sky::ui::UIRect &rect)
        {
            (void)host;
            (void)field;
            (void)event;
            (void)rect;
            return false;
        }
        virtual bool OnMove(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event)
        {
            (void)host;
            (void)event;
            return false;
        }
        virtual bool OnUp(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event)
        {
            (void)host;
            (void)event;
            return false;
        }

        // Optional popup (drawn above all rows; gets pointer first).
        virtual bool HasPopup() const { return false; }
        virtual void PaintPopup(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context)
        {
            (void)host;
            (void)context;
        }
        virtual bool OnPopupPointer(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event)
        {
            (void)host;
            (void)event;
            return false;
        }
        virtual bool OnEscape(ReflectedWidgetHost &host)
        {
            (void)host;
            return false;
        }
    };

    // kind -> widget (singletons).
    class ReflectedWidgetRegistry {
    public:
        static ReflectedWidgetRegistry &Get();

        void Register(PropertyEditorKind kind, std::shared_ptr<ReflectedWidget> widget);
        ReflectedWidget *Find(PropertyEditorKind kind) const;
        ReflectedWidget *FindWithPopup() const;

    private:
        std::unordered_map<uint8_t, std::shared_ptr<ReflectedWidget>> widgets;
    };

} // namespace sky::editor
