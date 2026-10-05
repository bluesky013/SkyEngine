//
// Created on 2026/10/05.
//

#pragma once

#include <editor/shell/ReflectedWidget.h>

#include <vector>

namespace sky::editor {

    // Specialized widget for PropertyEditorKind::Enum: a dropdown listing the
    // reflected enum values. Registered for Enum so the form view no longer
    // special-cases enums.
    class EnumReflectedWidget : public ReflectedWidget {
    public:
        PropertyEditorKind Kind() const override { return PropertyEditorKind::Enum; }

        void Paint(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context, PropertyField &field,
                   const sky::ui::UIRect &rect) override;
        bool OnDown(ReflectedWidgetHost &host, PropertyField &field, const sky::ui::UIPointerEvent &event,
                    const sky::ui::UIRect &rect) override;

        bool HasPopup() const override { return openField != nullptr; }
        void PaintPopup(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context) override;
        bool OnPopupPointer(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event) override;
        bool OnEscape(ReflectedWidgetHost &host) override;

    private:
        void Open(PropertyField &field, const sky::ui::UIRect &control, const sky::ui::UIRect &bounds);
        int CurrentIndex(const PropertyField &field) const;
        void Select(ReflectedWidgetHost &host, int index);

        PropertyField *openField = nullptr;
        int            hoverItem = -1;
        sky::ui::UIRect popupRect;
        std::vector<sky::ui::UIRect> itemRects;
    };

} // namespace sky::editor
