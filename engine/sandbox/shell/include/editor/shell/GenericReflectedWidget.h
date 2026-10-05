//
// Created on 2026/10/04.
//

#pragma once

#include <editor/shell/ReflectedWidget.h>

#include <vector>

namespace sky::editor {

    // Default widget for the generic leaf kinds: Bool, Integer, Float, String,
    // Enum (dropdown) and Vector (N float components). Registered under all of
    // those kinds; specialized widgets (Color/Rotation/Asset) take precedence.
    class GenericReflectedWidget : public ReflectedWidget {
    public:
        PropertyEditorKind Kind() const override { return PropertyEditorKind::Unknown; }

        void Paint(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context, PropertyField &field,
                   const sky::ui::UIRect &rect) override;
        bool OnDown(ReflectedWidgetHost &host, PropertyField &field, const sky::ui::UIPointerEvent &event,
                    const sky::ui::UIRect &rect) override;
        bool OnMove(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event) override;
        bool OnUp(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event) override;

        bool HasPopup() const override { return enumField != nullptr; }
        void PaintPopup(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context) override;
        bool OnPopupPointer(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event) override;
        bool OnEscape(ReflectedWidgetHost &host) override;

    private:
        sky::ui::UIRect SliderRect(const sky::ui::UIRect &control) const;
        sky::ui::UIRect VectorCell(const sky::ui::UIRect &control, int index, int count) const;
        float Fraction(float x, float left, float right) const;
        static float ReadComponent(const PropertyField &field, int index);
        bool WriteComponent(ReflectedWidgetHost &host, PropertyField &field, int index, float value);
        void BeginScalarTextEdit(ReflectedWidgetHost &host, PropertyField &field);
        void OpenEnum(PropertyField &field, const sky::ui::UIRect &control);

        PropertyField *enumField = nullptr;
        int            hoverEnumItem = -1;
        sky::ui::UIRect enumPopupRect;
        std::vector<sky::ui::UIRect> enumItemRects;

        PropertyField *dragField = nullptr;
        sky::ui::UIRect dragControl;
        int             dragComponent = -1;
        double          dragValue = 0.0;
    };

} // namespace sky::editor
